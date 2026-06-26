# litedb SQL 解析到执行流程设计

## 1. 设计目标

本文描述 litedb 从 SQL 文本到执行结果的完整处理链路，覆盖：

```text
SQL Text
  -> Lexer
  -> Parser
  -> AST
  -> Binder / Semantic Analyzer
  -> Logical Plan
  -> Optimizer
  -> Physical Plan
  -> Executor
  -> Storage Engine
  -> Query Result
```

当前项目已经具备 Lexer、Parser、AST 基础框架，并支持数据库、集合、基础 CRUD、表达式和 `VECTOR(n)` 字段定义。后续阶段应围绕一个可执行的最小闭环继续推进：

```sql
CREATE COLLECTION users (
    id BIGINT NOT NULL,
    name VARCHAR(64),
    age INTEGER,
    embedding VECTOR(128)
);

INSERT INTO users VALUES (1, 'Tom', 18, [0.1, 0.2, ...]);

SELECT id, name FROM users WHERE age >= 18 LIMIT 10;
```

第一版目标不是实现完整数据库优化器，也不是一次性完成复杂持久化和分布式能力，而是打通：

```text
Parser -> Binder -> Plan -> Executor -> Storage Interface
```

之后再逐步扩展向量检索、索引、持久化和分布式调度。

## 2. 总体架构

### 2.1 分层职责

| 阶段 | 输入 | 输出 | 核心职责 |
| --- | --- | --- | --- |
| Lexer | SQL 字符串 | Token 流 | 识别关键字、标识符、字面量、操作符 |
| Parser | Token 流 | AST | 按语法生成抽象语法树 |
| Binder | AST + Catalog | Bound Statement / Bound Expression | 名称解析、类型推导、语义校验 |
| Planner | Bound Statement | Logical Plan | 将语义节点转成逻辑算子树 |
| Optimizer | Logical Plan + Statistics | Optimized Logical Plan | 谓词下推、投影裁剪、访问路径选择 |
| Physical Planner | Optimized Logical Plan | Physical Plan | 选择具体物理算子和执行策略 |
| Executor | Physical Plan | Query Result / Mutation Result | 拉取或推送执行算子，调用存储层 |
| Storage | Storage Request | Records / Status | 元数据、记录读写、扫描、索引访问 |

### 2.2 推荐模块划分

建议在 `internal/src/core` 下逐步增加：

```text
internal/src/core/
  parser/       # 已存在：Lexer、Parser、AST
  catalog/      # 数据库、集合、字段、索引元数据
  binder/       # 语义绑定、类型检查
  plan/         # LogicalPlan、PhysicalPlan、PlanNode
  optimizer/    # 规则优化、代价优化预留
  execution/    # Executor、Operator、ResultSet
  storage/      # StorageEngine 接口和单机实现
```

也可以先合并 `plan` 和 `optimizer`，等逻辑计划节点稳定后再拆开。第一版重点是让边界清晰，不必过早追求复杂架构。

## 3. Parser 阶段

### 3.1 输入输出

输入：

```text
SQL 字符串
```

输出：

```cpp
std::unique_ptr<ast::StatementNode>
```

Parser 只负责语法正确性，不负责集合是否存在、字段是否存在、类型是否匹配。

例如：

```sql
SELECT name FROM users WHERE age >= 18;
```

Parser 应生成：

```text
SelectStatement
  select_list:
    ColumnReferenceExpression(name)
  collection:
    users
  where:
    BinaryExpression(
      left: ColumnReferenceExpression(age),
      op: >=,
      right: LiteralExpression(18)
    )
```

### 3.2 Parser 不应该做的事情

Parser 不应该检查：

- `users` 是否存在
- `name`、`age` 是否是合法字段
- `age >= 18` 是否类型兼容
- `VECTOR(128)` 是否与插入的向量维度一致
- `ORDER BY` 字段是否在 collection 中

这些属于 Binder 阶段。

### 3.3 Parser 错误示例

Parser 只报告语法错误：

```sql
SELECT FROM users;
```

错误：

```text
Expected select item
```

```sql
CREATE COLLECTION users ();
```

错误：

```text
Expected at least one column definition
```

## 4. Binder 阶段

### 4.1 Binder 的定位

Binder 是 Parser 和 Planner 之间最重要的一层。它把“语法上合法”的 AST 转换成“语义上合法”的绑定结构。

Binder 依赖 Catalog。Catalog 提供：

- 当前数据库
- 数据库是否存在
- 集合 schema
- 字段定义
- 主键、唯一约束、默认值
- 后续索引和向量索引元数据

### 4.2 Binder 输入输出

输入：

```text
AST + SessionContext + Catalog
```

输出：

```text
BoundStatement
```

示例：

```text
SelectStatement(AST)
  -> BoundSelectStatement
       collection: CollectionId
       output_columns: BoundColumnRef[]
       filter: BoundExpression
       limit: optional<size_t>
```

绑定后的节点不应只保存字符串字段名，还应保存解析后的字段 ID、字段类型和所属集合。

### 4.3 名称解析

#### USE

```sql
USE demo;
```

Binder 校验：

- `demo` 数据库是否存在

绑定结果：

```text
BoundUseStatement(database_id)
```

#### SELECT

```sql
SELECT id, name FROM users WHERE age >= 18;
```

Binder 校验：

- 当前 database 是否已选择
- `users` 集合是否存在
- `id`、`name`、`age` 字段是否存在
- `WHERE` 表达式是否返回 boolean

绑定结果：

```text
BoundSelectStatement
  collection: users(CollectionId)
  projection:
    users.id: BIGINT
    users.name: VARCHAR(64)
  filter:
    BinaryExpression(
      left: users.age: INTEGER,
      op: >=,
      right: 18: INTEGER,
      result_type: BOOLEAN
    )
```

### 4.4 类型系统

建议定义执行层通用类型：

```cpp
enum class LogicalTypeId
{
    Null,
    Boolean,
    Integer,
    BigInt,
    Float,
    Double,
    Varchar,
    Vector,
};

struct LogicalType
{
    LogicalTypeId id;
    std::optional<std::size_t> parameter;
};
```

Parser 中的 `DataType` 可以在 Binder 阶段转换为执行层 `LogicalType`。

### 4.5 表达式绑定

表达式绑定需要完成：

- 字面量类型推导
- 字段引用解析
- 运算符类型检查
- 函数解析
- 隐式类型转换
- 返回类型推导

示例：

```sql
age + 1 >= 18
```

绑定过程：

```text
age       -> BoundColumnRef(users.age, INTEGER)
1         -> BoundLiteral(INTEGER)
age + 1   -> BoundBinaryExpression(INTEGER)
18        -> BoundLiteral(INTEGER)
>=        -> BoundComparisonExpression(BOOLEAN)
```

### 4.6 向量语义校验

对于向量数据库，Binder 必须尽早校验维度和元素类型。

```sql
CREATE COLLECTION items (
    id BIGINT NOT NULL,
    embedding VECTOR(3)
);

INSERT INTO items VALUES (1, [0.1, 0.2]);
```

错误：

```text
Vector dimension mismatch: column embedding expects 3 elements, got 2
```

第一版向量字段建议约束：

- `VECTOR(n)` 必须有正整数维度
- 向量字面量最终必须可转换为数值
- 插入和更新时维度必须匹配
- `WHERE embedding = [...]` 第一版可以不支持，或仅支持完全相等
- 相似度检索先通过函数表达式或后续专用语法引入

### 4.7 Binder 错误示例

```sql
SELECT missing FROM users;
```

错误：

```text
Column not found: missing
```

```sql
SELECT * FROM missing_collection;
```

错误：

```text
Collection not found: missing_collection
```

```sql
SELECT * FROM users WHERE name + 1 > 3;
```

错误：

```text
Operator + does not support VARCHAR and INTEGER
```

## 5. Catalog 设计

### 5.1 Catalog 职责

Catalog 是 Binder、Planner、Executor 和 Storage 之间共享的元数据入口。

第一版 Catalog 至少需要描述：

```text
DatabaseCatalog
  name
  id
  collections

CollectionCatalog
  name
  id
  columns
  primary_key

ColumnCatalog
  name
  id
  type
  nullable
  default_value
  unique
  comment
```

后续可以增加：

```text
IndexCatalog
VectorIndexCatalog
ShardCatalog
ReplicaCatalog
```

### 5.2 Catalog 与 Storage 的关系

Catalog 可以先由内存结构维护，持久化阶段再落盘。

第一版建议：

```text
CatalogManager
  -> InMemoryCatalogStore
```

第二版：

```text
CatalogManager
  -> DurableCatalogStore
       catalog.json / catalog.bin
       WAL
```

分布式阶段：

```text
CatalogManager
  -> MetadataStore
       local mode: file
       distributed mode: raft / external metadata service
```

## 6. Logical Plan 阶段

### 6.1 Logical Plan 定位

Logical Plan 表达“要做什么”，不关心具体怎么读数据、是否使用索引、是否并行。

例如：

```sql
SELECT id, name FROM users WHERE age >= 18 LIMIT 10;
```

逻辑计划：

```text
Limit(10)
  Projection(id, name)
    Filter(age >= 18)
      LogicalScan(users)
```

### 6.2 第一版 LogicalPlan 节点

建议第一版支持：

```text
LogicalCreateDatabase
LogicalCreateCollection
LogicalDropDatabase
LogicalDropCollection
LogicalShow
LogicalDescribe
LogicalInsert
LogicalUpdate
LogicalDelete
LogicalScan
LogicalFilter
LogicalProjection
LogicalOrderBy
LogicalLimit
```

其中 CRUD 可对应：

```text
INSERT:
  LogicalInsert(collection, rows)

SELECT:
  LogicalLimit
    LogicalOrderBy
      LogicalProjection
        LogicalFilter
          LogicalScan

UPDATE:
  LogicalUpdate
    LogicalFilter
      LogicalScan

DELETE:
  LogicalDelete
    LogicalFilter
      LogicalScan
```

### 6.3 Planner 输入输出

输入：

```text
BoundStatement
```

输出：

```text
std::unique_ptr<LogicalPlanNode>
```

Planner 不做复杂优化，只负责结构化转换。

### 6.4 Planner 示例

SQL：

```sql
SELECT id, name FROM users WHERE age >= 18 ORDER BY id DESC LIMIT 10;
```

Bound Statement：

```text
BoundSelect
  collection: users
  projection: [id, name]
  filter: age >= 18
  order_by: id DESC
  limit: 10
```

Logical Plan：

```text
LogicalLimit(limit=10)
  LogicalOrderBy(keys=[id DESC])
    LogicalProjection(columns=[id, name])
      LogicalFilter(predicate=age >= 18)
        LogicalScan(collection=users)
```

## 7. Optimizer 阶段

### 7.1 第一版优化器原则

第一版优化器应该是规则驱动、可选、保守的。没有统计信息时，不做复杂代价估算。

建议先实现：

- 谓词下推
- 投影裁剪
- 常量折叠
- 无效过滤消除
- Limit 下推预留

暂缓：

- Join 重排
- 成本模型
- 复杂索引选择
- 分布式计划拆分

### 7.2 谓词下推

原始计划：

```text
Projection(id, name)
  Filter(age >= 18)
    Scan(users)
```

可以保持不变，也可以在物理计划阶段把 filter 合并到 scan：

```text
PhysicalSeqScan(users, predicate=age >= 18, columns=[id, name])
```

### 7.3 投影裁剪

SQL：

```sql
SELECT id, name FROM users WHERE age >= 18;
```

需要读取字段：

```text
id, name, age
```

不需要读取：

```text
embedding
```

这对向量数据库很重要，因为向量字段通常较大。即使第一版使用行式存储，也应该在计划层记录需要读取的字段，为后续列式存储、向量分离存储做准备。

### 7.4 常量折叠

```sql
SELECT * FROM users WHERE age > 10 + 8;
```

优化为：

```text
age > 18
```

### 7.5 向量查询优化预留

后续支持向量查询后：

```sql
SELECT id FROM items
ORDER BY distance(embedding, [0.1, 0.2, ...])
LIMIT 10;
```

逻辑计划可能是：

```text
LogicalLimit(10)
  LogicalOrderBy(distance(embedding, query_vector))
    LogicalProjection(id)
      LogicalScan(items)
```

优化器可以识别为：

```text
LogicalVectorSearch(
    collection=items,
    vector_column=embedding,
    query_vector=[...],
    metric=L2,
    top_k=10
)
```

物理计划可以选择：

```text
PhysicalBruteForceVectorSearch
```

或者在存在索引时选择：

```text
PhysicalHnswVectorSearch
```

## 8. Physical Plan 阶段

### 8.1 Physical Plan 定位

Physical Plan 表达“具体怎么执行”。它将逻辑算子映射到具体执行算子。

例如：

```text
LogicalScan(users)
```

可以变成：

```text
PhysicalSeqScan(users)
PhysicalIndexScan(users, index=idx_age)
PhysicalVectorIndexScan(users, vindex=idx_embedding)
```

第一版只需要：

```text
PhysicalCreateDatabase
PhysicalCreateCollection
PhysicalDrop
PhysicalInsert
PhysicalSeqScan
PhysicalFilter
PhysicalProjection
PhysicalSort
PhysicalLimit
PhysicalUpdate
PhysicalDelete
```

### 8.2 物理计划选择策略

第一版：

```text
所有 SELECT 使用 SeqScan
所有 WHERE 在执行层逐行计算
所有 ORDER BY 使用内存排序
所有 LIMIT 在排序或过滤后应用
```

第二版：

```text
如果存在普通索引，选择 IndexScan
如果存在向量索引，选择 VectorIndexScan
如果 projection 不包含向量字段，避免读取向量 payload
```

分布式阶段：

```text
CoordinatorPhysicalPlan
  RemoteScan(shard_1)
  RemoteScan(shard_2)
  Merge
  Limit
```

## 9. Executor 阶段

### 9.1 执行模型

建议第一版采用 Volcano Iterator 模型：

```cpp
class PhysicalOperator
{
public:
    virtual void open() = 0;
    virtual std::optional<Tuple> next() = 0;
    virtual void close() = 0;
};
```

这种模型简单，适合逐步实现：

```text
LimitOperator
  ProjectionOperator
    FilterOperator
      SeqScanOperator
```

执行流程：

```text
open()
next()
next()
...
close()
```

### 9.2 SELECT 执行

计划：

```text
Limit(10)
  Projection(id, name)
    Filter(age >= 18)
      SeqScan(users)
```

执行：

```text
SeqScan 从 Storage 拉取记录
Filter 计算 WHERE 表达式
Projection 裁剪输出列
Limit 控制输出数量
ResultSet 返回给调用方
```

### 9.3 INSERT 执行

SQL：

```sql
INSERT INTO users (id, name, age) VALUES (1, 'Tom', 18);
```

Binder 负责补齐默认值并形成完整记录：

```text
id: 1
name: Tom
age: 18
embedding: NULL 或默认值
```

Executor 调用：

```text
storage.insert(collection_id, record)
```

返回：

```text
MutationResult { affected_rows = 1 }
```

### 9.4 UPDATE 执行

SQL：

```sql
UPDATE users SET age = age + 1 WHERE id = 1;
```

执行：

```text
SeqScan(users)
  -> Filter(id = 1)
  -> 对匹配记录计算 assignment
  -> storage.update(row_id, new_record)
```

注意：

- assignment 右侧表达式可能引用旧记录字段
- 更新向量字段时必须重新校验维度
- 后续存在索引时，需要同步维护索引

### 9.5 DELETE 执行

SQL：

```sql
DELETE FROM users WHERE age < 18;
```

执行：

```text
SeqScan(users)
  -> Filter(age < 18)
  -> storage.delete(row_id)
```

第一版可以使用 tombstone 或直接从内存容器移除。持久化后建议使用 tombstone + compaction。

## 10. Storage Interface

### 10.1 第一版接口目标

Storage 层先提供稳定接口，不必马上完成复杂持久化。

建议定义：

```cpp
class StorageEngine
{
public:
    virtual Status create_database(const CreateDatabaseRequest & request) = 0;
    virtual Status drop_database(const DropDatabaseRequest & request) = 0;

    virtual Status create_collection(const CreateCollectionRequest & request) = 0;
    virtual Status drop_collection(const DropCollectionRequest & request) = 0;

    virtual Status insert(const InsertRequest & request) = 0;
    virtual ScanHandle scan(const ScanRequest & request) = 0;
    virtual Status update(const UpdateRequest & request) = 0;
    virtual Status remove(const DeleteRequest & request) = 0;
};
```

也可以将 Catalog 操作与 Record 操作拆成两个接口：

```text
CatalogStore
RecordStore
```

### 10.2 Record 表示

第一版建议用通用 Value：

```cpp
using VectorValue = std::vector<float>;

class Value
{
    LogicalType type;
    std::variant<
        std::monostate,
        bool,
        std::int32_t,
        std::int64_t,
        float,
        double,
        std::string,
        VectorValue
    > data;
};

using Tuple = std::vector<Value>;
```

为了便于更新和删除，每条记录需要内部 RowId：

```cpp
struct RowId
{
    std::uint64_t page_id;
    std::uint32_t slot_id;
};
```

内存版可以先简化为：

```cpp
using RowId = std::uint64_t;
```

### 10.3 InMemory Storage

第一版建议实现：

```text
InMemoryStorageEngine
  databases: map<string, Database>

Database
  collections: map<string, Collection>

Collection
  schema
  rows: vector<Row>
```

这可以快速验证 Binder、Planner、Executor 的正确性。

### 10.4 持久化 Storage

第二版再考虑：

```text
data/
  catalog/
    catalog.json
  databases/
    demo/
      users.schema
      users.data
      users.wal
```

记录格式可以先采用 append-only：

```text
RecordHeader
  row_id
  deleted
  payload_size

Payload
  column_count
  value_1
  value_2
  ...
```

后续再做：

- page manager
- buffer pool
- WAL
- checkpoint
- compaction
- vector payload 分离存储

## 11. Result 与错误模型

### 11.1 Result 类型

执行结果可以分为：

```text
QueryResult
  columns
  rows

MutationResult
  affected_rows

SchemaResult
  status
  optional rows
```

例如 `SHOW COLLECTIONS FROM demo` 和 `DESCRIBE users` 可以返回表格式结果。

### 11.2 错误分类

建议统一错误类型：

```text
ParseError
BindError
PlanError
OptimizeError
ExecutionError
StorageError
InternalError
```

每个错误至少包含：

```text
error code
message
optional SQL location
optional source stage
```

Parser 错误有 SQL 位置；Binder 错误也应尽量保留 AST 位置，便于定位用户 SQL 中的问题。

## 12. 端到端流程示例

### 12.1 CREATE COLLECTION

SQL：

```sql
CREATE COLLECTION users (
    id BIGINT NOT NULL,
    name VARCHAR(64),
    embedding VECTOR(128)
);
```

流程：

```text
Parser
  -> CreateCollectionStatement(AST)

Binder
  -> 校验当前 database
  -> 校验 collection 是否已存在
  -> 校验字段名是否重复
  -> 校验 VECTOR 维度
  -> BoundCreateCollection

Planner
  -> LogicalCreateCollection

Optimizer
  -> 不处理 DDL

Physical Planner
  -> PhysicalCreateCollection

Executor
  -> catalog.create_collection(...)
  -> storage.create_collection(...)

Result
  -> SchemaResult(ok)
```

### 12.2 INSERT

SQL：

```sql
INSERT INTO users (id, name, embedding)
VALUES (1, 'Tom', [0.1, 0.2, ...]);
```

流程：

```text
Parser
  -> InsertStatement(AST)

Binder
  -> 查找 users schema
  -> 绑定 id/name/embedding 字段
  -> 检查字段数量和值数量
  -> 检查 BIGINT/VARCHAR/VECTOR 类型
  -> 检查 vector 维度
  -> 补齐默认值
  -> BoundInsert

Planner
  -> LogicalInsert

Executor
  -> storage.insert(...)

Result
  -> MutationResult(affected_rows=1)
```

### 12.3 SELECT

SQL：

```sql
SELECT id, name
FROM users
WHERE age >= 18
ORDER BY id DESC
LIMIT 10;
```

流程：

```text
Parser
  -> SelectStatement(AST)

Binder
  -> 查找 users
  -> 绑定 id/name/age
  -> 推导 age >= 18 返回 BOOLEAN
  -> BoundSelect

Planner
  -> LogicalLimit
       LogicalOrderBy
         LogicalProjection
           LogicalFilter
             LogicalScan

Optimizer
  -> 投影裁剪：读取 id/name/age
  -> 谓词下推预留

Physical Planner
  -> PhysicalLimit
       PhysicalSort
         PhysicalProjection
           PhysicalFilter
             PhysicalSeqScan

Executor
  -> scan users
  -> filter age >= 18
  -> project id/name
  -> sort by id desc
  -> limit 10

Result
  -> QueryResult(columns=[id, name], rows=[...])
```

## 13. 向量查询扩展路线

### 13.1 第一阶段：向量字段存取

先支持：

```sql
CREATE COLLECTION items (
    id BIGINT NOT NULL,
    embedding VECTOR(3)
);

INSERT INTO items VALUES (1, [0.1, 0.2, 0.3]);
SELECT id, embedding FROM items;
```

目标：

- Binder 校验维度
- Storage 可保存向量字段
- Executor 可返回向量字段

### 13.2 第二阶段：暴力相似度查询

可以先通过函数表达式表达：

```sql
SELECT id
FROM items
ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3])
LIMIT 10;
```

执行策略：

```text
SeqScan
  -> 计算每行 distance
  -> TopK
```

### 13.3 第三阶段：向量索引

引入语法：

```sql
CREATE VINDEX idx_embedding ON items(embedding) USING HNSW;
```

计划变化：

```text
LogicalVectorSearch
  -> PhysicalHnswVectorSearch
```

### 13.4 第四阶段：分布式向量查询

分布式 TopK 查询：

```text
Coordinator
  -> RemoteVectorSearch(shard_1, top_k=10)
  -> RemoteVectorSearch(shard_2, top_k=10)
  -> RemoteVectorSearch(shard_3, top_k=10)
  -> MergeTopK(global_top_k=10)
```

第一版分布式建议先做：

- 静态分片
- coordinator 路由
- shard 本地执行
- 查询结果合并

暂缓：

- 自动 rebalance
- 强一致分布式事务
- 复杂副本选主

## 14. 推荐实现顺序

### 14.1 MVP 1：语义和执行闭环

1. 定义 `Value`、`Tuple`、`LogicalType`
2. 定义 `CatalogManager`
3. 定义 `Binder`
4. 定义 `BoundStatement` 和 `BoundExpression`
5. 定义 `LogicalPlanNode`
6. 定义 `PhysicalOperator`
7. 实现 `InMemoryStorageEngine`
8. 打通 `CREATE COLLECTION`
9. 打通 `INSERT`
10. 打通 `SELECT * FROM collection`
11. 打通 `WHERE`
12. 打通 `Projection`
13. 打通 `UPDATE` 和 `DELETE`

### 14.2 MVP 2：基础查询能力

1. `ORDER BY`
2. `LIMIT/OFFSET`
3. 表达式求值完善
4. 默认值和约束
5. `SHOW` / `DESCRIBE`

### 14.3 MVP 3：持久化

1. Catalog 落盘
2. Collection 数据落盘
3. RowId 和记录编码
4. WAL
5. 启动恢复

### 14.4 MVP 4：向量检索

1. 向量字段读取裁剪
2. 距离函数
3. Brute-force TopK
4. 向量索引元数据
5. HNSW 或 IVF 接入

### 14.5 MVP 5：分布式

1. Shard 元数据
2. Coordinator
3. Remote Executor
4. 分片扫描
5. 分布式 TopK
6. 副本和容错

## 15. 测试策略

### 15.1 Parser 测试

已有 parser 测试继续保持：

- statement kind
- AST 字段
- 表达式优先级
- 错误位置

### 15.2 Binder 测试

新增：

- collection not found
- column not found
- duplicate column
- type mismatch
- vector dimension mismatch
- default value type mismatch
- `WHERE` 非 boolean

### 15.3 Planner 测试

新增：

- `SELECT` 转换为正确逻辑计划树
- `INSERT` 转换为 `LogicalInsert`
- `UPDATE` / `DELETE` 包含 filter
- `LIMIT`、`ORDER BY` 顺序正确

### 15.4 Executor 测试

新增端到端测试：

```text
CREATE COLLECTION
INSERT
SELECT
UPDATE
DELETE
SHOW
DESCRIBE
```

向量测试：

```text
insert vector ok
insert vector dimension mismatch
select vector field
distance order by
```

### 15.5 Storage 测试

内存版：

- create/drop database
- create/drop collection
- insert/scan
- update/delete
- row id stability

持久化版：

- reopen recovery
- WAL replay
- deleted row visibility
- corrupted record handling

## 16. 关键设计原则

1. Parser 保持纯语法层，不依赖 Catalog。
2. Binder 是所有名称解析、类型检查和 schema 校验的入口。
3. Planner 只做结构转换，不混入执行细节。
4. Optimizer 第一版保持规则化和保守化。
5. Executor 只执行 Physical Plan，不直接理解 SQL AST。
6. Storage 层通过接口暴露能力，先内存实现，再持久化。
7. 向量字段从第一版数据模型纳入，但向量索引可以后置。
8. 分布式能力必须建立在单机执行闭环之后。

## 17. 第一版落地边界

第一版建议完成：

```text
SQL Parser 已有
  -> Binder
  -> LogicalPlan
  -> PhysicalPlan
  -> Volcano Executor
  -> InMemoryStorage
```

支持 SQL：

```sql
USE demo;
CREATE DATABASE demo;
CREATE COLLECTION users (...);
INSERT INTO users VALUES (...);
SELECT * FROM users WHERE ... ORDER BY ... LIMIT ...;
UPDATE users SET ... WHERE ...;
DELETE FROM users WHERE ...;
SHOW DATABASES;
SHOW COLLECTIONS FROM demo;
DESCRIBE users;
```

暂不支持：

```text
JOIN
GROUP BY
事务
普通索引
向量索引
分布式执行
复杂成本优化
```

这样可以最快验证 litedb 的核心数据库闭环，并为轻量化向量数据库能力留下清晰扩展点。
