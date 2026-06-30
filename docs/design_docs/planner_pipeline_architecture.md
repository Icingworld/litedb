# Planner Pipeline Architecture Review

## 背景

当前 litedb 的 SQL 路径大致是：

```text
SQL text
  -> Parser / AST StatementNode
  -> Binder / BoundStatement
  -> Planner / StatementPlan
  -> Executor / ExecutionResult
```

在 planner 层，项目已经引入了 `logical::LogicalPlanNode`，但只有 `SELECT`、`UPDATE`、`DELETE` 使用关系型算子树：

- `QueryPlan` 持有 `LogicalPlanNode root`。
- `UpdatePlan` 持有 `LogicalPlanNode input` 和 assignment payload。
- `DeletePlan` 持有 `LogicalPlanNode input`。
- `INSERT VALUES`、DDL、`USE`、`SHOW`、`DESCRIBE` 仍是 statement 级 plan，不进入 logical operator tree。

这个设计方向总体是合理的，但当前命名容易制造一个概念混淆：`StatementPlan` 同时像“执行器入口的统一计划对象”，又像“查询优化意义上的 plan”。如果后续引入 optimizer、physical plan、prepared statement、事务、权限、EXPLAIN，这个混淆会放大。

## 结论

建议保留顶层统一的 plan/command envelope，但不要把所有语句都强行转换成关系代数算子树。

更具体地说：

- Parser 和 Binder 层继续统一处理所有 statement。这是合理的，因为所有 SQL 都需要语法、名字解析、类型检查、catalog 校验和错误位置。
- Planner 对外继续返回一个统一的顶层对象，例如 `StatementPlan`，更建议将其语义命名为 `PlannedStatement` 或 `ExecutableStatementPlan`。
- 顶层对象内部应该明确分流：关系数据流语句走 `logical::LogicalPlanNode`；DDL、session command、metadata command 走 command plan。
- `logical::LogicalPlanNode` 不应成为“所有 SQL 语句的公共 IR”。它应该只表达可优化的数据流或数据修改流。

换句话说，应该在 planner 内部完成分流，而不是在 binder 后直接绕过 planner；但分流后的结果仍由统一的顶层执行入口向后传递。

## 为什么不是更早分流

如果在 binder 后就把 DDL、DML、query 分开传给不同后端，会带来几个问题：

1. Engine 层需要知道太多 statement 类型。现在 `Session::execute_sql()` 可以稳定地遵循 parse、bind、plan、execute，边界清楚。
2. 未来 prepared statement、EXPLAIN、审计、权限、事务包装、profiling 都希望看到一个统一的“已绑定并已规划的语句产物”。
3. `SHOW`、`DESCRIBE` 这类语句虽然不是普通查询优化目标，但它们仍返回 row set。过早分流会让 result metadata、执行结果形态、错误传播重复实现。
4. DDL 也可能需要 planner 级决策，例如 `CREATE INDEX` 选择构建策略，`DROP DATABASE` 展开为多 collection/index 清理，`CREATE COLLECTION` 补默认 schema 属性。

所以，不建议让 parser/binder 后直接分裂成完全独立的执行路径。

## 为什么不是所有语句都进入 logical operator tree

把 DDL、`USE`、`SHOW`、`DESCRIBE` 都建成 logical operator tree 看似统一，但对 litedb 当前阶段不划算：

1. 关系代数算子有明确含义：scan、filter、projection、join、aggregate、sort、limit、sink 等。DDL 的核心是 catalog/storage mutation，不是 tuple stream transformation。
2. 优化器会被迫理解大量不可交换、不可重排、有副作用的 command operator，反而削弱 logical plan 的可推理性。
3. 执行器会把“拉取 row 的 pipeline”和“一次性 command side effect”混在同一个递归执行函数里。
4. 对 `USE` 这类 session state command，放入 logical tree 只会增加抽象成本。

大型系统也很少要求“所有 SQL 都必须是同一种关系代数”。它们通常统一在 statement/executable artifact 上，而不是统一在 query algebra 上。

## 开源数据库路径对照

### PostgreSQL

PostgreSQL 官方文档描述的 query path 是：parser 生成 query tree，rewrite 处理规则，planner/optimizer 生成 executor 使用的 query plan，executor 递归执行 plan tree。这里重点是“query”路径，而不是所有 utility command 都进入 optimizer。参考：[The Path of a Query](https://www.postgresql.org/docs/current/query-path.html)。

PostgreSQL 源码中也有单独的 utility command 处理路径，例如 `src/backend/tcop/utility.c` 的 `ProcessUtility` 体系。设计含义是：顶层 SQL 处理器统一接收语句，但 DDL/utility 并不走普通查询优化器。参考：[postgres/src/backend/tcop/utility.c](https://github.com/postgres/postgres/blob/master/src/backend/tcop/utility.c)。

可借鉴点：统一入口，内部区分 optimizable statement 与 utility statement。

### SQLite

SQLite 的官方架构文档说明，它会把 SQL text 编译为 bytecode，再由 virtual machine 执行。`sqlite3_stmt` 是单条 SQL 的 bytecode program 容器；parser 后的 code generator 会为不同 statement 生成 VM bytecode，其中 `where*.c` 服务于 `SELECT`、`UPDATE`、`DELETE` 的 WHERE 访问路径。参考：[Architecture of SQLite](https://www.sqlite.org/arch.html)。

可借鉴点：SQLite 在最终执行产物上高度统一，但代码生成阶段仍按 statement 类型分工；查询规划能力集中服务于真正需要访问路径选择的语句。

### DuckDB

DuckDB 更偏统一 logical operator 路线。它的 planner 在 `Planner::CreatePlan()` 中 bind `SQLStatement`，取得 `BoundStatement`，再把 `bound_statement.plan` 保存为 `LogicalOperator`。源码里 `CreatePlan(unique_ptr<SQLStatement>)` 对很多 statement type 统一进入 `CreatePlan(*statement)`。参考：[duckdb/src/planner/planner.cpp](https://github.com/duckdb/duckdb/blob/main/src/planner/planner.cpp)。

可借鉴点：如果系统后续希望 optimizer、profiling、serialization、prepared statement 都围绕 logical operator 做，统一 operator tree 是可行路线。但这要求 logical operator 层能自然表达 command、DDL、transaction、pragma 等非关系操作，并承担更复杂的不变量。

对 litedb 当前阶段而言，DuckDB 的路线偏重；除非你准备把 `logical` 扩展为完整 SQL IR，否则不建议直接模仿。

## 对当前设计的评价

当前设计的优点：

- `Session::execute_sql()` 的主干非常清楚：parse -> bind -> plan -> execute。
- `StatementPlan` 给 executor 提供了单一入口，利于错误处理和结果类型统一。
- 只有 `SELECT`、`UPDATE`、`DELETE` 使用 logical tree，避免了过度抽象。
- `UPDATE`、`DELETE` 将“找出目标行”表达为 logical input，这个方向是对的；它为索引访问、谓词下推、limit/delete、后续物理计划留了位置。

当前设计的主要问题：

- `StatementPlan` 的名字容易被理解成“关系型 plan”，但它实际上是 statement 级 executable description。
- `Planner::plan()` 当前是一个大 switch，同时承担 command lowering、query logical planning、mutation planning，后续会越来越厚。
- `UpdatePlan` / `DeletePlan` 的 side effect 在 statement plan 外壳里，input 在 logical tree 里；这在当前执行器可以接受，但如果引入 physical plan，mutation sink 的位置需要重新定义。
- `SHOW` / `DESCRIBE` 返回 row set，但它们现在是特殊 command。短期没问题，长期需要决定是否把系统 catalog 暴露为可查询数据源。
- `INSERT VALUES` 不走 logical tree 是合理的；但未来 `INSERT INTO ... SELECT ...` 会需要一个 input pipeline 和 insert sink。

## 推荐目标架构

建议把 planner 分成“统一出口，内部分类”的结构：

```text
BoundStatement
  -> Planner
       -> QueryPlanner
            SELECT -> QueryPlan(logical root)
       -> MutationPlanner
            INSERT VALUES -> InsertValuesPlan
            UPDATE -> UpdatePlan(input logical root, assignments)
            DELETE -> DeletePlan(input logical root)
            future INSERT SELECT -> InsertSelectPlan(input logical root, target)
       -> CommandPlanner
            DDL / USE / SHOW / DESCRIBE -> CommandPlan variants
  -> PlannedStatement / StatementPlan
  -> Executor
```

顶层类型建议按语义分组：

```text
PlannedStatement
  kind: Query | Mutation | Command
  location

QueryPlan
  root: LogicalPlanNode

MutationPlan
  kind: InsertValues | InsertSelect | Update | Delete
  target database/collection
  input: optional LogicalPlanNode
  mutation payload

CommandPlan
  kind: Use | CreateDatabase | CreateCollection | CreateIndex | Drop... | Show... | Describe...
  command payload
```

这不一定要求立刻大改类继承。可以先保持当前 `StatementPlanKind`，但在目录和命名上体现分组。

## 关于 UPDATE / DELETE 的两种路线

### 路线 A：维持当前 wrapper 模式

`UpdatePlan` / `DeletePlan` 继续作为 statement plan，内部持有 input logical tree。

优点：

- 改动小，符合当前 executor。
- side effect 的边界清楚：logical tree 只负责找到行，statement executor 负责修改。
- 适合当前还没有 physical operator / pipeline executor 的阶段。

缺点：

- 以后如果要做统一 pipeline、EXPLAIN、physical plan，会发现 update/delete 的根不在 logical tree 内。
- mutation 的优化规则不容易作为 logical rewrite 表达，例如把 delete sink 与 scan/filter 组合成物理 delete operator。

### 路线 B：引入 LogicalUpdate / LogicalDelete 作为 sink operator

将 `UPDATE` / `DELETE` 表达为 logical tree 根节点：

```text
LogicalDelete
  child: LogicalFilter
    child: LogicalScan

LogicalUpdate
  assignments
  child: LogicalFilter
    child: LogicalScan
```

优点：

- 更接近未来 physical plan 和 pipeline executor。
- EXPLAIN 输出自然：整条语句就是一棵 logical tree。
- mutation sink 可以成为优化器和物理规划器的显式对象。

缺点：

- 当前阶段会增加执行器复杂度。
- 需要明确 side effect operator 的优化约束，不能像普通 projection/filter 那样随意重排。

建议：短期采用路线 A；当你准备引入 physical plan 或 EXPLAIN logical plan 时，再迁移到路线 B。

## 关于 DDL / SHOW / DESCRIBE

DDL 应保留为 command plan，不建议建成普通 logical operator。

`SHOW` / `DESCRIBE` 有两条可能路线：

- 短期：继续 command plan，executor 直接从 catalog 组装 row set。
- 长期：如果 litedb 引入系统 catalog 表，例如 `lite_databases`、`lite_collections`、`lite_indexes`，可以把 `SHOW` / `DESCRIBE` lower 成普通 catalog query，或者让它们共享 catalog scan 的执行逻辑。

当前项目还没有系统表查询层，因此短期 command plan 更务实。

## 具体修改建议

### 第一阶段：只澄清边界，不大改行为

1. 保留 `Planner::plan()` 返回 `std::unique_ptr<StatementPlan>`。
2. 给 `StatementPlan` 添加文档：它是 statement 级执行计划，不等同于 logical operator tree。
3. 在 planner 内部拆私有函数：
   - `plan_query(BoundSelectStatement &)`
   - `plan_insert(BoundInsertStatement &)`
   - `plan_update(BoundUpdateStatement &)`
   - `plan_delete(BoundDeleteStatement &)`
   - `plan_command(BoundStatement &)`
4. 将 DDL、USE、SHOW、DESCRIBE 的 plan 构造移动到 `statement_command_planner` 或 `command_planner` 文件，降低 `planner.cpp` 的 switch 压力。
5. 将目录分组调整为概念更清楚的结构，可以先不移动文件，只在命名中体现：
   - `planner/logical`: 关系型 logical operators。
   - `planner/statement`: 顶层 statement plans。
   - `planner/command`: 可选，放 command lowering。
   - `planner/mutation`: 可选，放 insert/update/delete lowering。

### 第二阶段：为 physical plan 预留位置

1. 新增 `physical` namespace 时，不要让 DDL 进入普通 physical operator tree。
2. Query 和 mutation 可以 lower 到 physical pipeline：
   - query root -> physical source/transform/sink。
   - update/delete -> physical mutation sink + child pipeline。
   - insert values -> physical values source + insert sink，或保持 command executor 直到需要批量插入优化。
3. `StatementPlan` 顶层可以持有 logical 或 physical，取决于 planner 阶段划分。例如：

```text
BoundStatement
  -> LogicalStatementPlan
  -> OptimizedStatementPlan
  -> PhysicalStatementPlan
  -> Executor
```

在当前项目规模下，不需要一次引入这些层；但命名要避免堵死这条路。

### 第三阶段：EXPLAIN / prepared statement / metadata

当要支持这些功能时，顶层 `StatementPlan` 应该集中保存：

- statement kind；
- location；
- result columns；
- parameter metadata；
- read/write classification；
- catalog dependencies；
- logical/physical root 或 command payload；
- 是否需要事务写锁、DDL handler、session state mutation。

这也是为什么不建议太早在 engine 层分流：这些横切信息最好集中在顶层 planned statement 上。

## 推荐命名

如果愿意做一次轻量重命名，建议：

- `StatementPlan` -> `PlannedStatement` 或 `ExecutableStatement`
- `QueryPlan` -> `QueryStatementPlan`
- `InsertPlan` -> `InsertValuesPlan`
- `logical::LogicalPlanNode` 保持不变
- `LogicalPlanner` -> `RelationalPlanner` 或 `LogicalQueryPlanner`

如果暂时不想改动文件名，也至少在注释中明确：

```text
StatementPlan is the top-level executable description of one SQL statement.
Only relation-producing or relation-consuming statements own logical plan nodes.
Command statements are represented as statement-level command plans.
```

## 最终建议

对 litedb 当前阶段，我建议采用 PostgreSQL 式的“统一 SQL 生命周期 + planner 内部分流”，并吸收 SQLite 的“统一可执行产物”思想，不要直接采用 DuckDB 式“所有 statement 都是 logical operator”的路线。

也就是：

- 不要在 binder 后把 DDL 直接绕过 planner。
- 不要把 DDL 强塞进 `LogicalPlanNode`。
- 保留统一顶层 plan，但把它明确定位为 statement-level executable artifact。
- 让 `logical` 只表达可优化的数据流；`UPDATE` / `DELETE` 当前可继续使用 input logical tree，未来 physical pipeline 成熟后再迁移成 mutation sink。

这个方向既能保持当前代码简洁，也不会阻碍后续引入 optimizer、physical plan、EXPLAIN、prepared statement 和系统 catalog 查询。
