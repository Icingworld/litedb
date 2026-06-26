# litedb 向量索引设计与实现顺序

## 1. 设计目标

向量索引用于加速 `VECTOR(n)` 字段上的 TopK 相似度检索。第一版只实现 HNSW，不急于抽象出多个向量索引算法，但模块边界要允许以后加入 IVF、Flat、DiskANN 等实现。

当前代码已经有独立的 `internal/src/core/vindex` 模块雏形：

```text
VectorIndex
  └─ HnswIndex

VectorDistanceMetric
VectorSearchParameters
VectorSearchResult
VectorIndexManager
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
    schema::VectorValue vector;
};

struct VectorIndexUpdateBinding
{
    common::VIndexId index_id;
    std::optional<schema::VectorValue> old_vector;
    std::optional<schema::VectorValue> new_vector;
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

底层 `insert(index_id, vector, record_id)`、`erase(index_id, record_id)`、`search(...)` 可以继续保留，作为 manager 内部或测试使用的 primitive。

## 4. 推荐实现顺序

### 4.1 第一步：整理 vindex 模块内部 API

目标：

- 把当前 `VectorIndexManager` 从直接操作扩展为 `prepare_` / `on_` 生命周期接口。
- 保留直接操作函数作为底层 primitive。
- 明确 `VectorIndexDefinition` 和未来 catalog entry 的字段对应关系。

建议工作：

1. 增加 `VectorIndexKeyBinding` 和 `VectorIndexUpdateBinding`。
2. 在 manager 中记录 `collection_id`、`column_id`、`column_ordinal`、`dimension`。
3. 实现 `prepare_insert/update/delete`。
4. `prepare_update` 只在向量变化时生成需要维护的 binding。
5. 增加 manager 级测试，覆盖 insert/update/delete 的准备和维护流程。

验收标准：

- 不经过 SQL，只用 manager API 能完整模拟 collection 写入维护。
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
  2. VectorIndexManager.create_index
  3. scan existing records
  4. 为已有非 NULL vector 构建索引

DROP VINDEX:
  1. VectorIndexManager.drop_index
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
  1. VectorIndexManager.prepare_insert(collection_id, record_data)
  2. IndexManager.prepare_insert(...)
  3. storage.insert(record_data) -> record_id
  4. IndexManager.on_insert(record_id, scalar_bindings)
  5. VectorIndexManager.on_insert(record_id, vector_bindings)

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
VectorIndexManager.search
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

1. `refactor: add vector index manager lifecycle bindings`
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
- manager lifecycle bindings。
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

- DDL 创建后 manager 中存在索引。
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
