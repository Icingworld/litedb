# LiteDB Optimizer Design

## 1. 目标

Optimizer 的职责是把 LogicalPlanner 生成的逻辑数据流改写为更适合执行的计划，并在具备统计信息和物理算子之后选择访问路径。它不负责 SQL 语法解析、名称绑定、类型检查，也不直接执行存储操作。

当前 LiteDB 的 SQL 主链路可以保持为：

```text
SQL text
  -> Parser / AST
  -> Binder / BoundStatement
  -> LogicalPlanner / StatementPlan
  -> Optimizer / optimized StatementPlan
  -> Physical Planner
  -> Executor
```

第一版 optimizer 的目标不是一次实现完整的商业数据库优化器，而是建立一个边界清晰、可测试、可逐步扩展的优化层：

- 只优化真正的数据流计划：`SELECT`、`UPDATE` 输入、`DELETE` 输入。
- 暂时不把 DDL、`USE`、`SHOW`、`DESCRIBE` 强行塞进关系代数树。
- 第一阶段以规则优化为主，不依赖统计信息。
- 后续再接入统计信息、代价估算、索引访问路径、向量检索路径和物理计划选择。

## 2. 主流数据库优化器做什么

### 2.1 PostgreSQL 路线

PostgreSQL 的查询路径大致是 parser 生成 query tree，rewrite 做规则重写，planner/optimizer 生成 executor 使用的 plan tree。它的核心经验是：

- 查询优化主要服务于可优化查询，不要求所有 utility command 都进入同一棵关系代数树。
- 优化器需要枚举访问路径，例如顺序扫描、索引扫描、bitmap scan、join path。
- 代价模型使用统计信息估算行数、选择率、I/O 成本和 CPU 成本。
- planner 的输出已经比较接近 executor 可执行的 plan。

对 LiteDB 的借鉴：

- 保留统一 SQL 生命周期，但在 planner/optimizer 内部区分 query/mutation/command。
- 在没有统计信息前，不要伪装成成本优化器。
- 先把 predicate、projection、limit 等信息清晰地挂到逻辑计划上，为后续访问路径选择做准备。

### 2.2 SQLite 路线

SQLite 会把 SQL 编译为虚拟机 bytecode，查询优化集中在 WHERE 子句、索引选择、连接顺序、排序和 limit 等决策上。它的核心经验是：

- 最终执行产物高度统一，但不同 statement 类型的编译路径仍然分工明确。
- 对单机轻量数据库来说，实用的启发式规则和小型代价模型通常比复杂优化框架更重要。
- WHERE 子句分析、索引可用性判断、排序消除是早期收益最高的优化点。

对 LiteDB 的借鉴：

- 第一版不需要 Cascades 级别的搜索框架。
- 先把 `WHERE column = literal`、范围谓词、ORDER BY/LIMIT 等模式识别出来。
- 物理层成熟前，optimizer 可以先只产出“更好的逻辑树”和 scan hints。

### 2.3 DuckDB 路线

DuckDB 更偏向统一 logical operator 路线，Binder 之后大量 statement 都会落到 logical operator，再经过规则优化和物理计划生成。它的核心经验是：

- 统一 logical operator 有利于 EXPLAIN、profiling、向量化执行和优化规则复用。
- 代价是 logical 层需要自然表达 command、DDL、copy、pragma 等非普通查询语句。
- 对分析型数据库，projection pruning、filter pushdown、join reorder、aggregate pushdown、向量化 pipeline 是核心优化点。

对 LiteDB 的借鉴：

- 可以学习它的规则组织方式和 logical/physical 分层。
- 当前不建议直接模仿“所有 statement 都是 logical operator”，因为 LiteDB 现有 `StatementPlan` 已经自然区分 command、mutation、query。

### 2.4 Volcano/Cascades 路线

Volcano/Cascades 优化器把优化过程建模为表达式组、等价规则、实现规则和代价搜索。它适合复杂 join、子查询、分布式计划和多种物理实现并存的数据库。

核心组件通常包括：

- memo：保存逻辑等价表达式和物理实现。
- transformation rules：逻辑等价改写，例如 join commutativity、predicate pushdown。
- implementation rules：把逻辑算子映射为物理算子，例如 logical scan -> seq scan / index scan。
- cost model：为候选计划估算代价并选择最优计划。

对 LiteDB 的借鉴：

- 不建议第一版引入 memo，否则工程复杂度会压过收益。
- 当 LiteDB 支持多表 join、多个索引、分布式执行和向量检索路径竞争时，再考虑 Cascades 风格框架。

## 3. LiteDB 当前边界

当前仓库里已经存在：

```text
internal/src/core/binder
internal/src/core/logical_plan
internal/src/core/executor
internal/src/core/index
internal/src/core/vindex
```

已有 logical node 主要是：

```text
LogicalScan
LogicalFilter
LogicalProjection
LogicalOrderBy
LogicalLimit
```

已有 statement-level plan 分组大致是：

```text
Command:
  USE / CREATE / DROP / SHOW / DESCRIBE

Mutation:
  INSERT
  UPDATE with logical input
  DELETE with logical input

Query:
  SELECT with logical root
```

Optimizer 应该遵守这个边界：

- `QueryPlan`：优化其 logical root。
- `UpdatePlan`：优化其 input logical root，assignment payload 不参与普通关系代数重排。
- `DeletePlan`：优化其 input logical root。
- `InsertPlan`：第一版不优化；未来 `INSERT INTO ... SELECT ...` 再引入 input pipeline。
- Command plans：第一版直接原样返回。

## 4. Optimizer 的输入输出

推荐第一版接口保持简单：

```cpp
namespace litedb::core::optimizer
{

class Optimizer
{
public:
    [[nodiscard]]
    std::expected<std::unique_ptr<planner::plan::StatementPlan>, OptimizerError> optimize(
        std::unique_ptr<planner::plan::StatementPlan> plan
    ) const;
};

} // namespace litedb::core::optimizer
```

内部按 plan kind 分流：

```text
StatementPlan
  -> Optimizer
       -> QueryOptimizer
            QueryPlan.root
       -> MutationOptimizer
            UpdatePlan.input
            DeletePlan.input
       -> Command passthrough
  -> StatementPlan
```

第一版可以原地重建 logical tree，也可以把 logical node 改为支持 move-based rewrite。不要为了 optimizer 过早引入共享所有权，计划树仍应保持 `std::unique_ptr` 单所有者模型。

## 5. 第一版规则优化

第一版规则优化只做语义保持的确定性改写，不做依赖统计信息的选择。

### 5.1 常量折叠

将可以在编译期求值的表达式提前计算：

```sql
SELECT * FROM users WHERE age > 10 + 8;
```

优化为：

```text
Filter(age > 18)
  Scan(users)
```

适用范围：

- 字面量上的算术、比较、布尔表达式。
- 确定性内置函数。
- 不访问列、不访问运行时状态、不依赖随机数或时间的表达式。

第一版可以先只折叠字面量二元表达式和一元表达式。函数常量折叠等函数系统标注 deterministic 后再做。

### 5.2 布尔表达式简化

典型规则：

```text
true AND x  -> x
false AND x -> false
true OR x   -> true
false OR x  -> x
NOT true    -> false
NOT false   -> true
```

对 Filter 的影响：

```text
Filter(true)
  child
```

可以消除为：

```text
child
```

```text
Filter(false)
  child
```

可以改写为：

```text
LogicalEmptyResult
```

如果第一版没有 `LogicalEmptyResult`，可以暂时保留 `Filter(false)`，但要在文档和 TODO 中明确后续节点。

### 5.3 谓词下推

当前 LiteDB 暂时没有 join/aggregate/subquery，逻辑树通常是：

```text
Limit
  OrderBy
    Projection
      Filter
        Scan
```

第一版 Planner 已经把 Filter 放在 Projection 下面、Scan 上面，因此谓词下推主要是为未来扩展保留规则框架。后续支持 join 后，谓词下推会变成高收益规则：

```text
Filter(users.age > 18 AND orders.price > 100)
  Join(users.id = orders.user_id)
    Scan(users)
    Scan(orders)
```

改写为：

```text
Filter(users.id = orders.user_id)
  Join
    Filter(users.age > 18)
      Scan(users)
    Filter(orders.price > 100)
      Scan(orders)
```

第一版可以实现两个轻量规则：

- 合并相邻 Filter：`Filter(a) -> Filter(b) -> child` 合并为 `Filter(a AND b) -> child`。
- 将 Filter 放到 Projection 之下，前提是 predicate 只引用 Projection 输入中仍可解析的列，不引用 projection alias 或 computed output。

### 5.4 投影裁剪

投影裁剪的目标是减少 scan 需要读取的列，尤其是避免读取大向量字段。

示例：

```sql
SELECT id, name FROM users WHERE age >= 18;
```

最终 scan 至少需要：

```text
id, name, age
```

不需要：

```text
embedding
```

推荐做法：

1. 从计划根节点向下传递 required columns。
2. `Projection` 把输出表达式引用的列加入 required columns。
3. `Filter` 把 predicate 引用的列加入 required columns。
4. `OrderBy` 把排序表达式引用的列加入 required columns。
5. 到 `Scan` 时保存 required columns。

这需要给 `LogicalScan` 增加 projected column metadata，例如：

```cpp
class LogicalScan final : public LogicalPlanNode
{
public:
    void set_required_columns(std::vector<common::ColumnId> columns);
    const std::vector<common::ColumnId> & required_columns() const noexcept;
};
```

注意：投影裁剪不是简单删除 `LogicalProjection`。`Projection` 仍然决定输出列、输出顺序、别名和表达式计算。`Scan.required_columns` 只决定底层读取哪些输入列。

### 5.5 Limit 下推和 TopK 预留

在没有索引和物理 TopK 算子前，Limit 下推的收益有限，但可以保留 logical hint：

```text
Limit(10)
  OrderBy(score DESC)
    child
```

未来可以改写为：

```text
TopK(order_by=score DESC, limit=10)
  child
```

第一版建议：

- 不跨 `OrderBy` 强行下推 `Limit`，否则会改变语义。
- 可以把 `Limit` 信息传给 `OrderBy`，未来物理计划选择 bounded sort。
- 对没有 `OrderBy` 的 `Limit -> Scan`，未来可以让 SeqScan 在 executor 层提前停止。

### 5.6 访问路径标注

当前 `LogicalScan` 只表示“扫描某个 collection”。当索引能力接入 optimizer 后，需要判断：

```sql
SELECT * FROM users WHERE age = 18;
```

如果存在 `idx_users_age`，可以选择：

```text
PhysicalIndexScan(users, idx_users_age, key=18)
```

第一版不直接选择物理索引扫描，但可以设计中间结构：

```cpp
struct AccessPathCandidate
{
    AccessPathKind kind; // SeqScan, ScalarIndexScan, VectorIndexScan
    std::optional<common::IndexId> index_id;
    std::vector<PredicateId> consumed_predicates;
    CostEstimate estimated_cost;
};
```

在没有统计信息时，选择策略可以很保守：

- 等值谓词命中唯一索引：优先 index scan。
- 等值谓词命中普通 BTree：可以 index scan。
- 范围谓词命中 BTree：数据量未知时谨慎启用，或者先只生成候选不自动选择。
- 其他情况：SeqScan。

## 6. 统计信息与代价模型

成本优化器需要统计信息。没有统计信息时，复杂代价模型只会制造虚假的确定性。

### 6.1 需要的统计信息

Collection 级别：

```text
row_count
row_width
data_pages
last_analyze_time
```

Column 级别：

```text
null_count
distinct_count
min_value
max_value
histogram
average_width
```

Index 级别：

```text
index_type
column_ids
entry_count
height
leaf_pages
clustered / covering capability
```

Vector index 级别：

```text
vector_column_id
dimension
metric
index_type
entry_count
build_options
recall / latency hints
```

### 6.2 第一版 CostEstimate

建议先定义一个简单结构，不急着把所有字段用起来：

```cpp
struct CostEstimate
{
    double startup_cost = 0.0;
    double total_cost = 0.0;
    double estimated_rows = 0.0;
    double estimated_row_width = 0.0;
};
```

粗略代价：

```text
SeqScan:
  total_cost = row_count * cpu_tuple_cost + data_pages * seq_page_cost

IndexScan:
  total_cost = index_probe_cost + estimated_rows * cpu_tuple_cost + random_page_cost

Sort:
  total_cost = input_cost + rows * log2(rows) * cpu_operator_cost

Limit:
  estimated_rows = min(input_rows, limit)
```

第一版即使不使用这些值，也可以把结构和测试钩子预留出来。

## 7. 向量查询优化

LiteDB 已经有 vector index 相关模块，optimizer 需要为向量检索预留明确路线。

### 7.1 暴力 TopK 形态

SQL 可能是：

```sql
SELECT id
FROM items
ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3])
LIMIT 10;
```

普通逻辑计划：

```text
Limit(10)
  OrderBy(l2_distance(embedding, query_vector))
    Projection(id)
      Scan(items)
```

第一阶段可以由物理计划生成：

```text
PhysicalTopK
  PhysicalProjection
    PhysicalSeqScan
```

### 7.2 向量索引访问路径

当存在 HNSW 等向量索引时，optimizer 可以识别：

```text
ORDER BY distance(vector_column, constant_query_vector)
LIMIT k
```

并生成候选：

```text
VectorIndexScan(collection=items, index=idx_embedding, top_k=10)
```

需要满足：

- distance 函数与 index metric 匹配。
- query vector 是常量，或 prepared statement 参数已经可绑定维度。
- `LIMIT` 存在且为正数。
- vector column 维度与索引维度一致，这通常由 Binder 或 index metadata 保证。

### 7.3 标量过滤与向量检索组合

未来常见查询：

```sql
SELECT id
FROM items
WHERE category = 'book'
ORDER BY l2_distance(embedding, ?)
LIMIT 10;
```

可能路径：

```text
1. VectorIndexScan top_k=N -> Filter(category) -> TopK(10)
2. ScalarIndexScan(category) -> BruteForceVectorTopK(10)
3. SeqScan -> Filter(category) -> BruteForceVectorTopK(10)
```

没有统计信息时不应武断选择。需要 category 选择率、向量索引召回和候选集大小策略。第一版可以只支持“无额外过滤谓词的 vector top-k”。

## 8. 推荐目录结构

```text
internal/src/core/optimizer/
  CMakeLists.txt
  optimizer.hpp
  optimizer.cpp
  optimizer_error.hpp

  rule/
    optimizer_rule.hpp
    rule_set.hpp
    constant_folding_rule.hpp
    boolean_simplification_rule.hpp
    filter_merge_rule.hpp
    projection_pruning_rule.hpp

  rewrite/
    logical_rewriter.hpp
    expression_rewriter.hpp
    column_usage_collector.hpp

  stats/
    statistics_provider.hpp
    table_statistics.hpp
    column_statistics.hpp
    index_statistics.hpp

  cost/
    cost_estimate.hpp
    cost_model.hpp

  access_path/
    access_path.hpp
    access_path_enumerator.hpp
```

第一阶段可以只落地：

```text
optimizer.hpp/.cpp
optimizer_error.hpp
rule/optimizer_rule.hpp
rule/rule_set.hpp
rewrite/expression_rewriter.hpp
rewrite/column_usage_collector.hpp
```

`stats`、`cost`、`access_path` 可以先写接口和 TODO，等索引/physical planner 成熟后再接入。

## 9. Rule 执行模型

推荐采用固定点迭代，但必须设置上限，避免规则互相反复改写：

```text
for pass in 1..max_passes:
  changed = false
  for rule in rules:
    result = rule.apply(plan)
    changed |= result.changed
  if !changed:
    break
```

规则应满足：

- 单条规则只负责一种清晰改写。
- 每次改写都保持语义等价。
- 规则执行顺序稳定，debug 输出可复现。
- 每条规则有单元测试，覆盖命中和不命中两类场景。

推荐第一版规则顺序：

```text
1. ConstantFoldingRule
2. BooleanSimplificationRule
3. FilterMergeRule
4. ProjectionPruningRule
5. LimitHintRule
```

`ProjectionPruningRule` 放后面，因为它依赖表达式已经尽量简化。

## 10. EXPLAIN 与调试

Optimizer 很容易引入“结果正确但计划不可理解”的问题。建议在实现 optimizer 前或同时补齐：

- logical plan debug printer 展示每个节点。
- optimized logical plan debug printer 展示 required columns、limit hint、access path hint。
- 后续 EXPLAIN 可以显示：

```text
Logical Plan:
  Limit(10)
    OrderBy(id DESC)
      Projection(id, name)
        Filter(age >= 18)
          Scan(users)

Optimized Logical Plan:
  Limit(10)
    OrderBy(id DESC, limit_hint=10)
      Projection(id, name)
        Filter(age >= 18)
          Scan(users, required_columns=[id, name, age])
```

如果引入 rule trace，可以记录：

```text
Rule ConstantFoldingRule changed Filter(age > 10 + 8) to Filter(age > 18)
Rule ProjectionPruningRule set Scan(users).required_columns=[id,name,age]
```

## 11. 测试策略

### 11.1 表达式重写测试

覆盖：

- `10 + 8 -> 18`
- `true AND x -> x`
- `false OR x -> x`
- `NOT true -> false`
- 不折叠非确定性函数。
- 不折叠含列引用的表达式。

### 11.2 逻辑计划重写测试

覆盖：

- 相邻 Filter 合并。
- `Filter(true)` 消除。
- `Filter(false)` 在没有 `LogicalEmptyResult` 前保持不破坏语义。
- `Projection`、`Filter`、`OrderBy` 共同决定 scan required columns。
- `Limit -> OrderBy` 生成 limit hint，但不改变排序语义。

### 11.3 StatementPlan 分流测试

覆盖：

- `QueryPlan` 会进入 QueryOptimizer。
- `UpdatePlan` 只优化 input，不改 assignment。
- `DeletePlan` 只优化 input。
- `InsertPlan` 第一版 passthrough。
- command plans passthrough。

### 11.4 端到端回归

所有 optimizer 改写都必须保证查询结果不变。建议使用同一批 SQL 对比：

```text
optimizer disabled result == optimizer enabled result
```

第一版可以用 session 或 planner option 控制：

```cpp
struct OptimizerOptions
{
    bool enabled = true;
    bool enable_constant_folding = true;
    bool enable_projection_pruning = true;
};
```

## 12. 实施顺序

### 阶段 1：框架和 passthrough

目标：

- 新增 `internal/src/core/optimizer`。
- `Optimizer::optimize()` 接收并返回 `StatementPlan`。
- Query/Update/Delete 能识别 logical root/input。
- Command/Insert passthrough。
- 接入 engine 主链路，但默认行为不变。

验收：

- 全部现有 planner/executor 测试通过。
- optimizer disabled/enabled 输出结果一致。

### 阶段 2：表达式级规则

目标：

- 常量折叠。
- 布尔简化。
- Filter(true) 消除。

验收：

- 表达式重写单元测试。
- logical debug printer 能看出改写结果。

### 阶段 3：列裁剪

目标：

- column usage collector。
- `LogicalScan.required_columns`。
- projection/filter/order_by 共同决定 required columns。

验收：

- SELECT 不读取无关大列的计划信息可见。
- executor 暂时不消费 required columns 也可以，但 plan debug 必须正确。

### 阶段 4：访问路径候选

目标：

- 根据标量索引 metadata 识别等值/range predicate。
- 生成 seq scan / index scan candidate。
- 无统计信息时只做保守选择。

验收：

- 有索引和无索引时候选路径不同。
- 结果语义不变。

### 阶段 5：物理计划与代价模型

目标：

- 引入 physical plan。
- `AccessPathCandidate` 转 physical scan。
- 接入基础统计信息。
- 选择 seq scan / index scan / vector index scan。

验收：

- EXPLAIN 能显示 logical、optimized logical、physical plan。
- 代价选择有稳定测试。

## 13. 关键原则

1. Optimizer 不做 Binder 的事：不解析名称、不做类型检查、不修正非法 SQL。
2. Optimizer 不直接做 Executor 的事：不访问 storage，不产生结果行。
3. 第一版只做规则优化，不假装拥有统计信息。
4. `StatementPlan` 是顶层执行计划容器，`LogicalPlanNode` 只表达可优化的数据流。
5. DDL/SHOW/DESCRIBE 第一版 passthrough，未来有系统表后再考虑 lower 成 catalog query。
6. 投影裁剪应影响 scan 的 required columns，不应删除输出 Projection 语义。
7. 所有规则必须可关闭、可测试、可通过 debug printer 观察。
8. 向量索引优化先从 `ORDER BY distance(vector, constant) LIMIT k` 这个明确模式开始。

## 14. 推荐 MVP

LiteDB 当前最合适的 optimizer MVP 是：

```text
Optimizer framework
  + statement plan passthrough
  + query/update/delete logical root rewrite
  + constant folding
  + boolean simplification
  + projection pruning metadata
```

暂缓：

```text
Join reorder
Subquery decorrelation
Cascades memo
Full cost-based optimizer
Distributed planning
Complex vector + scalar hybrid path selection
```

这样能先把 optimizer 插入现有架构，并为 scalar index、vector index、physical plan 和 EXPLAIN 留出稳定扩展点。
