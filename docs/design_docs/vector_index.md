# litedb 向量索引设计与实现顺序

> 状态说明：第 1～8 节记录向量索引最初的接入路线，其中部分目标已经完成；第 9 节描述 HNSW 与持久化完成后的当前 `VectorIndexEngine` 重构目标，并作为本轮实现的执行依据。

## 1. 设计目标

向量索引用于加速 `VECTOR(n)` 字段上的 TopK 相似度检索。第一版只实现 HNSW，不急于抽象出多个向量索引算法，但模块边界要允许以后加入 IVF、Flat、DiskANN 等实现。

当前代码已经有独立的 `internal/src/core/vindex` 模块雏形：

```text
VectorIndex
  └─ HnswIndex

VectorDistanceMetric
VectorSearchParameters
VectorSearchResult
VectorIndexEngine
VectorIndexError
```

下一阶段目标不是立刻写完整 HNSW 图算法，而是先让向量索引进入数据库核心生命周期：

```text
catalog 记录索引定义
  -> SQL 可以创建、删除、查看索引
  -> 写入路径自动维护索引
  -> 查询计划可以选择向量索引
  -> HNSW 内存实现替换当前线性搜索骨架
  -> 持久化阶段从数据重建索引
```

## 2. 非目标

第一轮向量索引接入暂不实现：

- 多种向量索引算法。
- 持久化 HNSW 图文件。
- 增量崩溃恢复。
- 分布式向量索引。
- 复杂成本优化器。
- 向量索引和标量索引的复杂联合优化。
- 多向量字段的自动索引推荐。

这些能力应等单机 catalog、SQL、执行和内存索引生命周期稳定后再推进。

## 3. 核心语义

### 3.1 索引对象

向量索引是 collection 下某个 `VECTOR(n)` 列的二级索引。

建议第一版元数据：

```text
VectorIndexEntry
  index_id: VIndexId
  database_id: DatabaseId
  collection_id: CollectionId
  column_id: ColumnId
  index_name: string
  index_kind: Hnsw
  metric: L2 | InnerProduct | Cosine
  dimension: size_t
  hnsw:
    max_neighbors
    ef_construction
    ef_search_default
    random_seed
```

其中 `dimension` 必须等于列类型 `VECTOR(n)` 的 `n`。

### 3.2 查询语义

第一版推荐使用已有距离函数表达向量查询：

```sql
SELECT id, title
FROM docs
ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3])
LIMIT 10;
```

优化器识别该模式后选择向量索引：

```text
ORDER BY distance(vector_column, constant_query_vector) LIMIT k
  -> VectorIndexSearch(collection, vindex, query_vector, top_k)
```

后续可以再增加专用语法：

```sql
SELECT id, title
FROM docs
VECTOR SEARCH embedding <-> [0.1, 0.2, 0.3]
USING idx_embedding
LIMIT 10;
```

不建议第一版直接引入专用语法。先复用函数表达式，能减少 parser 和 binder 的语法面。

### 3.3 写入维护语义

向量索引和标量索引一样，应分成两个阶段：

```text
prepare_*:
  从 RecordData 提取 vector key
  校验列类型和维度
  比较 update 前后向量是否变化

on_*:
  storage mutation 成功后
  使用 record_id 维护索引实例
```

建议接口：

```cpp
struct VectorIndexKeyBinding
{
    common::VIndexId index_id;
    common::VectorValue vector;
};

struct VectorIndexUpdateBinding
{
    common::VIndexId index_id;
    std::optional<common::VectorValue> old_vector;
    std::optional<common::VectorValue> new_vector;
    bool vector_changed {false};
};
```

```cpp
prepare_insert(collection_id, record_data)
on_insert(record_id, bindings)

prepare_update(collection_id, old_record_data, new_record_data)
on_update(record_id, bindings)

prepare_delete(collection_id, old_record_data)
on_delete(record_id, bindings)
```

底层 `insert(index_id, vector, record_id)`、`erase(index_id, record_id)`、`search(...)` 可以继续保留，作为 engine 内部或测试使用的 primitive。

## 4. 推荐实现顺序

### 4.1 第一步：整理 vindex 模块内部 API

目标：

- 把当前向量索引运行时入口从直接操作扩展为 `prepare_` / `on_` 生命周期接口。
- 保留直接操作函数作为底层 primitive。
- 明确 `VectorIndexDefinition` 和未来 catalog entry 的字段对应关系。

建议工作：

1. 增加 `VectorIndexKeyBinding` 和 `VectorIndexUpdateBinding`。
2. 在 engine 中记录 `collection_id`、`column_id`、`column_ordinal`、`dimension`。
3. 实现 `prepare_insert/update/delete`。
4. `prepare_update` 只在向量变化时生成需要维护的 binding。
5. 增加 engine 级测试，覆盖 insert/update/delete 的准备和维护流程。

验收标准：

- 不经过 SQL，只用 engine API 能完整模拟 collection 写入维护。
- 维度错误在 `prepare_*` 阶段失败。
- storage mutation 后的 `on_*` 不再做复杂语义判断。

### 4.2 第二步：扩展 catalog 元数据

目标：

- 让数据库知道“有哪些向量索引存在”。
- 先持久化定义，索引实例仍然内存构建。

建议新增或扩展：

```text
CatalogVectorIndexKind
  Hnsw

CatalogVectorDistanceMetric
  L2
  InnerProduct
  Cosine

VectorIndexEntry
```

Catalog API：

```cpp
create_vector_index(...)
drop_vector_index(...)
find_vector_index(...)
list_vector_indexes(collection_id)
```

语义约束：

- 索引名在同一个 collection 内唯一。
- 目标列必须存在。
- 目标列必须是 `VECTOR(n)`。
- `dimension` 来自列类型，不允许 SQL 手动写出不同维度。
- 第一版同一列允许多个 metric 的索引，但建议先限制为同一列同一 metric 只能有一个索引。

验收标准：

- catalog 测试能创建、查询、列出、删除向量索引。
- catalog snapshot / persistence 如果已有索引元数据格式，也要纳入向量索引字段。

### 4.3 第三步：SQL DDL 支持

目标：

- 支持创建、删除和查看向量索引。

建议语法：

```sql
CREATE VINDEX idx_embedding
ON docs(embedding)
USING HNSW
WITH (
    metric = 'l2',
    max_neighbors = 16,
    ef_construction = 200
);

DROP VINDEX idx_embedding ON docs;

SHOW VINDEXES FROM docs;
```

如果想降低第一版 parser 复杂度，可以先不支持 `WITH`：

```sql
CREATE VINDEX idx_embedding ON docs(embedding) USING HNSW;
```

默认参数：

```text
metric = L2
max_neighbors = 16
ef_construction = 200
ef_search_default = 64
random_seed = 0
```

需要新增：

- lexer 关键字：`VINDEX`、`HNSW`、`WITH` 可选。
- AST：`CreateVectorIndexStatement`、`DropVectorIndexStatement`、`ShowVectorIndexesStatement`。
- parser 测试：语法字段完整性。

验收标准：

- Parser 只负责语法，不查 catalog。
- AST 能表达 index name、collection name、column name、kind、options。
- 不支持的 option 在 parser 或 binder 阶段给出明确错误。

### 4.4 第四步：Binder 支持

目标：

- 把 DDL AST 绑定为语义正确的 bound statement。

Binder 校验：

- 当前 database 已选择。
- collection 存在。
- column 存在。
- column 类型是 `VECTOR(n)`。
- index name 不冲突。
- `USING` 只能是 `HNSW`。
- metric 名称合法。
- HNSW 参数为正数，并满足基本关系。

建议 HNSW 参数约束：

```text
dimension > 0
max_neighbors > 0
ef_construction >= max_neighbors
ef_search_default > 0
```

绑定结果建议包含：

```text
BoundCreateVectorIndexStatement
  database_id
  collection_id
  column_id
  column_ordinal
  index_name
  definition
```

验收标准：

- 对非 VECTOR 列创建向量索引会失败。
- 对不存在列创建向量索引会失败。
- 对重复索引名创建会失败。
- 绑定后的 statement 不再依赖字符串查找。

### 4.5 第五步：Planner 和 Executor 支持 DDL

目标：

- SQL DDL 能真正创建/drop/list 向量索引。

建议计划节点：

```text
CreateVectorIndexPlan
DropVectorIndexPlan
ShowVectorIndexesPlan
```

Executor 流程：

```text
CREATE VINDEX:
  1. catalog.create_vector_index
  2. VectorIndexEngine.create_index
  3. scan existing records
  4. 为已有非 NULL vector 构建索引

DROP VINDEX:
  1. VectorIndexEngine.drop_index
  2. catalog.drop_vector_index

SHOW VINDEXES:
  1. catalog.list_vector_indexes
  2. 返回表格式结果
```

注意顺序：

- 如果 catalog 先创建成功，但内存索引构建失败，需要删除 catalog entry 或返回可恢复错误。
- 第一版可以在全局数据库锁下执行，避免并发写入期间建索引。

验收标准：

- 对已有数据创建索引后，立刻可以查询到已有记录。
- 删除索引后，查询计划回退到 brute-force。
- 重复创建、删除不存在索引都有明确错误。

### 4.6 第六步：写入路径自动维护

目标：

- INSERT / UPDATE / DELETE 自动同步维护向量索引。

推荐流程：

```text
INSERT:
  1. VectorIndexEngine.prepare_insert(collection_id, record_data)
  2. IndexEngine.prepare_insert(...)
  3. storage.insert(record_data) -> record_id
  4. IndexEngine.on_insert(record_id, scalar_bindings)
  5. VectorIndexEngine.on_insert(record_id, vector_bindings)

UPDATE:
  1. storage.get(record_id) -> old_record
  2. 构造 new_record
  3. prepare_update(old_record, new_record)
  4. storage.update(record_id, new_record)
  5. on_update(record_id, bindings)

DELETE:
  1. storage.get(record_id) -> old_record
  2. prepare_delete(old_record)
  3. storage.erase(record_id)
  4. on_delete(record_id, bindings)
```

第一版没有事务时，尽量把可能失败的类型和维度检查放在 storage mutation 之前。`on_*` 阶段理论上只应发生内存错误或内部状态错误。

验收标准：

- 插入后向量索引可查。
- 更新向量后旧结果消失，新结果可查。
- 更新非向量列不重建向量索引项。
- 删除后搜索不返回已删除记录。

### 4.7 第七步：查询语法和执行接入

目标：

- 让用户能通过 SQL 使用向量索引。

第一版推荐识别：

```sql
SELECT id, title
FROM docs
ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3])
LIMIT 10;
```

也要支持带过滤：

```sql
SELECT id, title
FROM docs
WHERE category = 'paper'
ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3])
LIMIT 10;
```

执行策略可以分两阶段：

第一阶段：

```text
有 WHERE:
  先 SeqScan + Filter
  再 brute-force TopK

无 WHERE:
  如果有匹配 metric 的向量索引
    使用 VectorIndexSearch
  否则 brute-force TopK
```

第二阶段：

```text
有 WHERE:
  先向量索引取更多候选 ef_search/top_k_factor
  回表后做 WHERE 二次过滤
  不足时扩大候选或回退 brute-force
```

第一版建议先保守处理 `WHERE`，避免 ANN 候选不足导致语义明显错误。

建议新增 logical node：

```text
LogicalVectorSearch
  collection_id
  vindex_id
  vector_column_id
  query_vector
  metric
  limit
```

Executor：

```text
VectorIndexEngine.search
  -> record_ids with distance
  -> storage.get(record_id)
  -> 可选 Filter 二次校验
  -> Projection
```

验收标准：

- 没有索引时查询结果与 brute-force 一致。
- 有索引时计划选择 `LogicalVectorSearch`。
- 返回结果按距离升序稳定排序。
- 相同距离时按 `RecordId` 稳定排序。

### 4.8 第八步：实现真正 HNSW

目标：

- 替换当前 `HnswIndex` 内部的线性搜索骨架。

建议分阶段：

```text
阶段 1:
  实现节点、层级、邻接表、随机层高
  insert 构建图
  search_layer 和 top-k 搜索
  不实现删除物理清理，先 tombstone

阶段 2:
  update = tombstone old + insert new
  周期性 rebuild 清理 tombstone
  支持 ef_search 参数

阶段 3:
  优化 neighbor selection
  增加召回率测试
  增加大数据量性能测试
```

核心结构建议：

```text
HnswNode
  record_id
  vector
  level
  deleted
  neighbors_by_level: vector<vector<RecordId>>

HnswIndex
  entry_point
  max_level
  nodes
  options
```

验收标准：

- 小数据集结果与 brute-force 完全一致。
- 固定 random_seed 下构建结果稳定。
- 中等数据集召回率达到文档约定阈值。
- 删除和更新不会返回 tombstone 节点。

### 4.9 第九步：持久化和重建

目标：

- 数据库重启后能恢复向量索引能力。

第一版推荐只持久化向量索引定义：

```text
启动:
  load catalog vector index definitions
  for each vector index:
    create in-memory HNSW
    scan collection records
    extract vector
    insert(record_id)
```

暂不持久化 HNSW 图。

验收标准：

- 重启后 catalog 仍能列出向量索引。
- 重启后向量查询仍能走索引。
- 数据损坏或某条记录向量维度异常时有明确恢复错误。

## 5. 推荐提交拆分

建议按以下粒度提交，避免一次 PR 过大：

1. `refactor: add vector index engine lifecycle bindings`
2. `feat: add vector index catalog metadata`
3. `feat: parse create and drop vindex statements`
4. `feat: bind vector index ddl statements`
5. `feat: execute vector index ddl statements`
6. `feat: maintain vector indexes on record mutations`
7. `feat: plan vector topk search from distance order by`
8. `feat: implement in-memory hnsw search graph`
9. `feat: rebuild vector indexes from persisted records`

每个提交都应有对应测试，优先使用 subsystem tests，端到端 SQL 测试放在跨模块行为稳定后补。

## 6. 测试策略

### 6.1 vindex 单元测试

覆盖：

- 维度校验。
- 三种距离度量。
- insert/search/update/delete。
- engine lifecycle bindings。
- HNSW 固定随机种子的确定性。

### 6.2 catalog 测试

覆盖：

- 创建向量索引元数据。
- 重复名称失败。
- list/find/drop。
- catalog snapshot / persistence。

### 6.3 parser / binder 测试

覆盖：

- `CREATE VINDEX` 基础语法。
- `DROP VINDEX`。
- `SHOW VINDEXES`。
- 非 VECTOR 列创建失败。
- 不合法 HNSW 参数失败。

### 6.4 planner / executor 测试

覆盖：

- DDL 创建后 engine 中存在索引。
- 对已有记录建索引。
- INSERT / UPDATE / DELETE 自动维护。
- `ORDER BY l2_distance(...) LIMIT k` 选择向量搜索。
- 无索引时回退 brute-force。

### 6.5 端到端测试

建议 SQL：

```sql
CREATE DATABASE demo;
USE demo;
CREATE COLLECTION docs (
    id BIGINT NOT NULL,
    category VARCHAR(32),
    embedding VECTOR(3)
);

INSERT INTO docs VALUES (1, 'a', [0.0, 0.0, 0.0]);
INSERT INTO docs VALUES (2, 'a', [1.0, 0.0, 0.0]);
INSERT INTO docs VALUES (3, 'b', [5.0, 0.0, 0.0]);

CREATE VINDEX idx_docs_embedding ON docs(embedding) USING HNSW;

SELECT id
FROM docs
ORDER BY l2_distance(embedding, [0.2, 0.0, 0.0])
LIMIT 2;
```

期望结果：

```text
1
2
```

## 7. 关键取舍

1. 先接生命周期，再写完整 HNSW。
   这样可以尽早发现 catalog、SQL、executor 的边界问题。

2. 先持久化定义，暂不持久化图。
   HNSW 图文件涉及版本、删除、恢复和一致性，应该后置。

3. 先复用距离函数语法，暂不引入专用查询语法。
   用户可以马上用 `ORDER BY l2_distance(...) LIMIT k`，优化器再把它识别成向量搜索。

4. 有过滤条件时先保守。
   `WHERE + ANN` 很容易出现候选不足问题。第一版可以先 brute-force 或做二次校验，等语义稳定后再优化。

5. `prepare_` / `on_` 生命周期值得补。
   向量索引最终会进入写入路径，直接操作接口不足以支撑一致性和预校验。

## 8. 最小可用里程碑

可以把向量索引 MVP 定义为：

```text
1. CREATE VINDEX / DROP VINDEX / SHOW VINDEXES 可用
2. catalog 保存向量索引定义
3. 对已有数据建索引
4. INSERT / UPDATE / DELETE 自动维护索引
5. ORDER BY l2_distance(vector_col, const_vector) LIMIT k 可以走索引
6. 重启后从持久化 records 重建内存索引
```

达到这个里程碑后，再投入真正 HNSW 性能优化和图持久化会更稳。

## 9. VectorIndexEngine 升级方案

### 9.1 当前状态与重构目标

截至当前实现，Flat 与 HNSW backend、HNSW 图持久化、写入生命周期维护和数据库重启恢复已经具备。现有 `VectorIndexManager` 实际承担了大部分 engine 职责，但元数据解释、全量恢复以及恢复失败后的重建策略仍散落在 `DatabaseEngine` 中。

本轮重构的目标不是简单地把 `Manager` 改名为 `Engine`，而是建立清晰、完整的向量索引子系统边界：

1. 使用 `VectorIndexEngine` 作为 `vindex` 模块唯一的运行时入口。
2. 由 engine 统一管理多个向量索引实例的创建、打开、恢复、重建和删除。
3. 由 engine 将 `VectorIndexEntry` 与 `CollectionSchema` 转换成运行时描述，禁止转换逻辑泄漏到 `DatabaseEngine`。
4. 由 engine 负责 DML 写入维护、单个子系统内的失败回滚以及搜索路由。
5. 由 engine 提供原子化 `restore_all`，避免恢复失败后发布部分索引状态。
6. 隐藏 Flat、HNSW 及其持久化实现细节，使上层只依赖稳定的领域接口。
7. 在命名、构造方式、恢复接口和运行时视图上与 `IndexEngine` 保持一致。

### 9.2 Engine 的职责边界

`VectorIndexEngine` 负责：

- 管理数据库内所有向量索引的运行时实例。
- 校验向量索引元数据、列类型、列序号和维度。
- 根据元数据创建或打开具体 backend。
- 从 `StorageEngine` 扫描已有记录并构建或重建索引。
- 根据 collection ID 和 vindex ID 路由写入与搜索请求。
- 执行 `prepare_insert/update/delete` 与 `on_insert/update/delete`。
- 对一次操作涉及的多个向量索引执行子系统内部回滚。
- 管理 HNSW 文件路径、打开、删除、一致性验证和可恢复重建。
- 提供只读运行时状态视图。

`VectorIndexEngine` 不负责：

- 创建、提交或回滚 catalog 元数据事务。
- 直接执行 SQL DDL 或物理计划。
- 协调 Storage、标量索引与向量索引之间的整体事务。
- 实现 HNSW 图算法、文件编码和底层文件读写协议。

跨模块操作仍由 `DatabaseEngine` 协调；DML 中 Storage、`IndexEngine` 和 `VectorIndexEngine` 的调用顺序仍由 `Executor` 协调。

### 9.3 目标分层

```text
DatabaseEngine                         跨子系统生命周期与 DDL 协调
  ├─ MetaEngine                        元数据及其持久化
  ├─ StorageEngine                     collection 与 record 存储
  ├─ IndexEngine                       标量索引子系统
  └─ VectorIndexEngine                 向量索引子系统入口
       ├─ VectorIndexStore             单个运行时向量索引实例
       │    └─ FlatIndex               精确搜索 backend
       └─ VectorIndexStore
            └─ HnswIndex               ANN backend
                 └─ HnswStore          HNSW 物理持久化
```

各层职责如下：

- `VectorIndexEngine` 管理多个实例、生命周期、路由和恢复策略。
- `VectorIndexStore` 持有单个索引的运行时描述与 `VectorIndex` backend。
- `VectorIndex` 是算法接口，`FlatIndex` 和 `HnswIndex` 是具体实现。
- `HnswStore` 只负责 HNSW 文件格式和物理读写，不承担多索引管理职责。

建议新增 `VectorIndexDescriptor`，至少包含：

```cpp
struct VectorIndexDescriptor
{
    common::VIndexId index_id;
    common::CollectionId collection_id;
    common::ColumnId column_id;
    std::size_t column_ordinal;
    std::size_t dimension;
    VectorIndexKind kind;
    VectorDistanceMetric metric;
};
```

当前公开的 `VectorIndexDefinition` 与 `meta::entry::VectorIndexEntry` 字段重复。重构后应删除该公共定义，由 engine 内部根据 meta entry 和 collection schema 构造 `VectorIndexDescriptor`。

### 9.4 VectorIndexEngine 接口

推荐公共接口：

```cpp
class VectorIndexEngine
{
public:
    VectorIndexEngine(
        std::filesystem::path data_directory,
        filesystem::FileSystem & filesystem
    ) noexcept;

    std::expected<void, VectorIndexError> create_index(
        const meta::entry::VectorIndexEntry & index_entry,
        const schema::CollectionSchema & collection_schema,
        const storage::StorageEngine & storage
    );

    std::expected<void, VectorIndexError> restore_all(
        const meta::MetaEngine & catalog,
        const storage::StorageEngine & storage
    );

    std::expected<void, VectorIndexError> drop_index(common::VIndexId index_id);
    std::expected<void, VectorIndexError> drop_collection_indexes(
        common::CollectionId collection_id
    );

    // prepare_insert/on_insert
    // prepare_update/on_update
    // prepare_delete/on_delete

    std::expected<std::vector<VectorSearchResult>, VectorIndexError> search(
        common::VIndexId index_id,
        const VectorIndexKey & query,
        VectorSearchRequest request
    ) const;

    std::optional<ManagedVectorIndexView> find_index(
        common::VIndexId index_id
    ) const noexcept;

    std::vector<ManagedVectorIndexView> list_indexes(
        common::CollectionId collection_id
    ) const;

    void clear() noexcept;
};
```

接口调整原则：

| 当前接口 | 目标接口 | 说明 |
| --- | --- | --- |
| `create_index(VectorIndexDefinition)` | `create_index(entry, schema, storage)` | engine 直接解释权威元数据 |
| `restore_index(definition)` | 私有 backend 操作 | 单索引恢复不向上层暴露 |
| `rebuild_index(definition)` | 私有恢复策略 | 是否重建由 engine 判断 |
| 无 `restore_all` | `restore_all(meta, storage)` | 完整接管启动恢复 |
| 公共 `insert/erase` | `VectorIndexStore` 接口或 engine 私有接口 | 防止绕过 DML 协调破坏一致性 |
| `prepare_* / on_*` | 保留 | engine 的写入维护职责 |
| `search/find/list` | 保留 | engine 的查询路由和状态查询职责 |

`ManagedVectorIndexView` 不应继续暴露 `const VectorIndex &`。运行时视图应只返回 `index_id`、`collection_id`、`column_id`、`column_ordinal`、`kind`、`metric`、`dimension` 和 `entry_count` 等只读信息；所有查询必须通过 engine 路由。

### 9.5 创建、恢复和重建语义

创建流程：

```text
DatabaseEngine 暂存 VectorIndexEntry
  -> VectorIndexEngine.create_index(entry, schema, storage)
       -> 校验列与维度
       -> 构造 descriptor
       -> 创建 backend
       -> 扫描已有 records 构建索引
       -> 成功后发布到 engine
  -> DatabaseEngine 提交 meta
  -> meta 提交失败时调用 engine.drop_index 回滚
```

启动恢复必须使用临时 engine：

```cpp
VectorIndexEngine restored {data_directory_, *filesystem_};

// 遍历 catalog，恢复或重建全部向量索引。
// 只有全部成功后才发布：
*this = std::move(restored);
```

该方式保证：

- 第 N 个索引失败时不会留下前 N - 1 个已发布、其余索引缺失的半恢复状态。
- `restore_all` 失败时，调用前已有的 engine 状态保持不变。
- `DatabaseEngine` 不再了解 `restore_index -> rebuild_index` 的内部策略。

恢复错误应分类处理：

- HNSW 文件缺失、内容损坏或与 Storage 数据不一致：允许从 Storage 重建。
- metadata/schema 非法、维度不一致、算法或度量不支持：直接失败。
- 文件权限、目录创建和 I/O 写入失败：直接失败，不能用重建掩盖真实故障。

现有 `StorageFailure` 无法可靠表达以上差异。应扩展 `VectorIndexErrorCode`，至少区分无效索引列或元数据、文件缺失、数据损坏、索引过期以及文件系统错误；也可以使用仅在恢复流程内部可见的细分结果，再统一转换为公开错误。

### 9.6 需要修改的模块

`internal/src/core/vindex`：

- `vector_index_manager.hpp/.cpp` 重命名为 `vector_index_engine.hpp/.cpp`。
- `VectorIndexManager` 重命名为 `VectorIndexEngine`。
- 新增 `vector_index_store.hpp/.cpp` 与 `VectorIndexDescriptor`。
- 将元数据转换、单索引恢复、重建和 backend 创建收为 engine 私有实现。
- `indexes_by_id_` 改为持有 `VectorIndexStore`。
- 更新 HNSW 中对 `VectorIndexManager` 的 friend 声明，优先通过公开校验接口减少 friend。
- 更新 CMake source 列表，并为 `litedb_vindex` 增加所需的 meta 依赖。

`internal/src/core/database`：

- 成员 `vector_index_manager_` 改为 `vector_index_engine_`。
- accessor `vector_index_manager()` 改为 `vector_index_engine()`。
- 删除 `runtime_metric`、`vector_definition` 和 `restore_vector_indexes_from_meta`。
- 初始化统一调用 `vector_index_engine_.restore_all(meta_, storage_)`。
- CREATE/DROP VINDEX 以及 DROP COLLECTION/DATABASE 改为调用新 engine。
- staged meta、meta commit 和跨子系统回滚继续留在 `DatabaseEngine`。

`internal/src/core/executor`：

- 构造参数、成员和辅助执行函数统一改为 `VectorIndexEngine`。
- INSERT/UPDATE/DELETE 的两阶段维护顺序保持不变。
- 测试不再依赖“只有 StorageEngine 的半配置 manager”；测试 fixture 应显式构造并注入 engine。

测试与文档：

- `vector_index_tests.cpp` 改为 engine 级测试，并使用 meta entry 与 schema 创建索引。
- `database_recovery_tests.cpp` 改用 `vector_index_engine()` accessor。
- 更新所有 Manager 命名、调用链、错误文本和测试名称。
- Parser、Binder、Logical Plan、Physical Plan、meta 持久化格式和现有 HNSW 文件格式不因本次重构改变。

### 9.7 实施顺序

1. 提取 `VectorIndexDescriptor` 和 `VectorIndexStore`，保持现有行为不变。
2. 将 Manager 文件与类型重命名为 `VectorIndexEngine`。
3. 改造 `create_index`，由 engine 直接消费 meta entry、schema 和 storage。
4. 实现临时 engine 模式的原子化 `restore_all`，迁入恢复与重建策略。
5. 替换 `DatabaseEngine`、`Executor` 及测试中的依赖名称和构造方式。
6. 收紧 `insert`、`erase`、`restore_index` 和 `rebuild_index` 的可见性。
7. 细化恢复错误分类，补齐失败路径测试。
8. 更新模块说明和调用链文档。

当前处于 vindex 重构阶段，默认采用一次性干净迁移，不长期保留：

```cpp
using VectorIndexManager = VectorIndexEngine;
```

只有存在明确的外部 API 兼容需求时，才增加带废弃标记的短期别名。

### 9.8 验收标准

完成升级后必须满足：

1. `vindex` 模块对外不再出现 `VectorIndexManager`。
2. `DatabaseEngine` 中不存在向量 metric 转换、运行时 definition 构造和逐索引恢复循环。
3. `VectorIndexEngine::restore_all` 失败不会发布部分恢复结果，也不会破坏调用前状态。
4. HNSW 文件缺失或索引过期可以按策略从 Storage 重建。
5. 非法 metadata/schema 和不可恢复文件系统错误不会被错误地当作可重建状态。
6. CREATE/DROP VINDEX、DROP COLLECTION/DATABASE 的行为和回滚语义保持不变。
7. INSERT/UPDATE/DELETE 继续同时维护标量与向量索引，并通过现有失败路径回滚。
8. Flat 与 HNSW 搜索结果、HNSW 持久化格式及重启恢复行为不发生兼容性退化。
9. 运行时视图不暴露具体 backend 引用。
10. vindex、executor、database 相关测试以及完整 CTest 全部通过。
