# litedb 标量索引设计

## 1. 设计目标

标量索引用于加速普通字段上的等值查询和范围查询。第一阶段目标不是直接实现完整的持久化数据库索引，而是先把索引在数据库核心中的生命周期、维护逻辑和查询接入跑通。

当前推荐路线：

```text
先内存索引接入核心流程
  -> 再持久化索引定义
  -> 再从持久化 record 重建内存索引
  -> 最后实现真正的持久化 B+Tree
```

不要在第一次接入时同时实现 page-based B+Tree、WAL 和崩溃恢复。那些属于更后面的存储引擎问题。

## 2. 当前索引模块

`internal/src/core/index` 提供标量索引键、纯内存索引实现、单索引 `IndexStore` 和数据库级 `IndexEngine`。一个 `IndexStore` 对应一个索引实例，`IndexEngine` 负责索引生命周期与自动维护。

核心结构：

```text
ScalarIndex
  ├─ HashIndex
  └─ OrderedScalarIndex
       └─ MapIndex（BTREE 的过渡后端）

BTreeIndex -> BTreePageStore（页式 B+Tree，正在实现）

ScalarIndexKey
  ├─ ScalarIndexEqual
  └─ ScalarIndexLess

IndexRange
IndexError

IndexStore -> ScalarIndex backend
IndexEngine -> [IndexStore, IndexStore, ...]
```

索引内部存储结构：

```text
ScalarIndexKey -> [RecordId, RecordId, ...]
```

第一版是非唯一索引，因此同一个 key 可以对应多条记录。

`ScalarIndexKey` 保存精确物理类型，不执行跨数值类型归一化。例如 `INT(1)`、`BIGINT(1)` 和 `DOUBLE(1.0)` 是不同的键。索引只对应一个固定类型的列，上游必须先根据列类型完成转换。这样可以避免通过 `long double` 比较导致的平台精度差异，并为后续稳定的磁盘编码保留明确类型标签。

NULL 不进入标量索引，NaN 在键构造阶段被拒绝；正零和负零比较相等。不同类型之间仅保留确定性排序以满足容器比较器要求，不表示 SQL 层允许混合类型索引。

`erase` 必须接收 `record_id`：

```cpp
erase(key, record_id)
```

原因是非唯一索引下只传 key 不知道应该删除哪条记录。例如：

```text
18 -> [1, 2, 5, 9]
```

删除 `record_id = 2` 后应变成：

```text
18 -> [1, 5, 9]
```

这也适合 update 维护流程：

```text
erase(old_key, record_id)
insert(new_key, record_id)
```

同一个 key 可以对应不同的 `RecordId`，但完全相同的 `(key, RecordId)` 只能存在一次；重复插入返回 `DuplicateEntry`。

## 3. 内存索引实现

### 3.1 HashIndex

`HashIndex` 使用 `std::unordered_map` 提供等值查询，不属于 `OrderedScalarIndex`，因此它在类型层面不暴露 `scan_range`。当前 SQL 语法仍不暴露 `USING HASH`；catalog 和内部运行时可以保存 Hash 索引定义。

### 3.2 MapIndex

`MapIndex` 是 `OrderedScalarIndex` 的纯内存过渡实现，后端使用：

```cpp
std::map<ScalarIndexKey, std::vector<RecordId>, ScalarIndexLess>
```

支持：

```text
insert
erase
find_equal
scan_range
```

它保持现有 BTREE 的有序 key、等值查询和范围查询行为，待页式 `BTreeIndex` 接入 `IndexEngine` 后废弃。

`IndexStore` 记录索引列的 `LogicalType`，写入、删除和查询入口都会校验键的精确物理类型，并在单个索引边界内维护唯一性约束。`IndexEngine` 对外提供按 `IndexId` 的 `find_equal` 和 `scan_range`，不暴露内部 `ScalarIndex` 引用。Hash 索引收到范围查询时返回 `UnsupportedRangeScan`。

后续如果实现数据库级索引，推荐最终实现成 B+Tree，而不是传统 B 树：

- 范围扫描更常见，B+Tree 的叶子链表更适合顺序扫描。
- 内部节点只存 separator key 和 child pointer，fanout 更高。
- record locator 统一放在叶子节点。
- 更贴近 page-based 存储实现。

未来形态：

```text
B+Tree
  internal node: separator keys + child page ids
  leaf node: key -> record ids
  leaf sibling links: range scan
```

## 4. 接入数据库核心的推荐流程

### 4.1 扩展 catalog 索引元数据

先让数据库知道“哪些索引存在”。

建议新增 `IndexEntry`：

```text
IndexEntry
  index_id
  database_id
  collection_id
  index_name
  column_id
  index_kind: BTree
  unique: false
```

第一版 `unique` 固定为 false，只做非唯一索引。

扩展 catalog API：

```cpp
create_index(...)
drop_index(...)
find_index(...)
list_indexes(collection_id)
```

SQL 可以随后接入：

```sql
CREATE INDEX idx_age ON users(age) USING BTREE;
CREATE INDEX idx_name ON users(name) USING BTREE;
DROP INDEX idx_age;
SHOW INDEXES FROM users;
```

如果实现成本需要拆分，也可以先只做 catalog API，再接 parser/binder/planner。

### 4.2 通过 StorageEngine 提供回表能力

索引查询返回的是 `RecordId`，因此 storage 必须支持按 id 读取记录：

```cpp
std::expected<schema::Record, StorageError> get(common::RecordId record_id) const;
```

这个接口有两个用途：

- IndexScan 拿到 `RecordId` 后回表读取完整 record。
- update/delete 维护索引前读取 old record。

Storage v2 由 `StorageEngine` 统一提供 `get/scan/insert/update/erase`，索引层不再接触 collection-level Store。

### 4.3 当前 IndexEngine 与 IndexStore 边界

不要把索引维护逻辑塞进 `BTreeIndex`，也不要让 executor 手动维护每个索引。

当前结构：

```text
IndexEngine
  collection_id -> [index ids]
  index_id -> IndexStore

IndexStore
  IndexDescriptor
  ScalarIndex backend
```

`IndexEngine` 职责：

```cpp
create_index(collection_schema, existing_records)
drop_index(index_id)
on_insert(record_id, record_data)
on_update(record_id, old_record_data, new_record_data)
on_delete(record_id, old_record_data)
find_indexes(collection_id)
```

`IndexStore` 只负责一个索引实例的键类型校验、唯一性约束、增删、等值查询和范围扫描，不依赖 `MetaEngine` 或 `StorageEngine`。

`create_index` 会从已有 records 全量构建索引：

```text
scan collection
  -> extract indexed column value
  -> ScalarIndexKey::from_value
  -> index.insert(key, record_id)
```

### 4.4 先实现内存索引自动维护

写入路径维护规则：

```text
insert:
  1. 预构造所有相关 index key
  2. storage insert 成功
  3. index insert(record_id)

delete:
  1. storage.get(record_id) 读取 old record
  2. 预构造所有相关 old key
  3. storage erase 成功
  4. index erase(old_key, record_id)

update:
  1. storage.get(record_id) 读取 old record
  2. 预构造 old key 和 new key
  3. storage update 成功
  4. indexed column changed 时执行:
     erase(old_key, record_id)
     insert(new_key, record_id)
```

第一版没有事务和 rollback。为了降低不一致风险，应在 storage mutation 前完成 key 构造和基本校验，避免 storage 已成功但 index 因 key 类型失败。

由于索引列来自 schema 标量列，正常情况下 key 构造不应失败。`VECTOR` 列不能创建标量索引。

### 4.5 查询执行接入 IndexScan

先只支持简单谓词：

```sql
SELECT * FROM users WHERE age = 18;
SELECT * FROM users WHERE age >= 18;
SELECT * FROM users WHERE age BETWEEN 18 AND 30;
```

识别形态：

```text
Filter(Comparison(ColumnRef, Literal))
  child = Scan(collection)
```

选择规则：

```text
=
  使用 BTreeIndex

< <= > >= BETWEEN
  只使用 BTreeIndex

其他表达式
  回退 SeqScan + Filter
```

建议新增 logical node：

```text
LogicalIndexScan
  collection_id
  index_id
  index_kind
  lookup key 或 range
```

执行路径：

```text
LogicalIndexScan
  -> index.find_equal / index.scan_range
  -> StorageEngine.get(collection_id, record_id)
  -> make_evaluation_record
  -> 后续 Projection / OrderBy / Limit
```

第一版可以保留原有 Filter 做二次校验，避免索引选择规则漏掉 NULL、类型转换或复杂表达式语义。

## 5. 持久化路线

### 5.1 第一阶段：只持久化索引定义

先把索引定义写入 catalog：

```text
index name
collection id
column id
index kind
unique flag
```

启动时：

```text
load catalog index definitions
for each index:
  scan persisted collection records
  rebuild in-memory index
```

这是最简单、最稳的持久化方案。它避免了索引文件格式、页管理和崩溃恢复问题，同时重启后仍能使用索引。

### 5.2 第二阶段：持久化索引文件

等核心接入稳定后，再实现真正的持久化结构：

```text
Hash index file
B+Tree index file
```

这时需要设计：

- 逻辑 leaf / internal page
- 固定 4096 字节 page layout 与 key serialization
- 单索引文件头、PageId 分配和节点页随机读写
- split / merge
- 空闲页回收
- crash consistency
- rebuild / repair
- 与 row log 或 WAL 的一致性

当前已经完成前三项基础设施：`BTreePage` 使用 `(ScalarIndexKey, RecordId)` 复合键处理重复标量键，
`BTreePageCodec` 负责单节点页与 4096 字节物理页之间的编解码，`BTreePageStore` 负责单索引文件头、
连续 PageId 分配、root/entry count 元数据以及节点页随机读写。页式 `BTreeIndex` 已负责创建、打开和持有
`BTreePageStore`；树遍历和修改算法尚未实现。运行时暂时仍使用 `MapIndex` 并在启动时重建。

### 5.3 B+Tree 删除策略

B+Tree 删除是复杂点。可以分阶段：

```text
阶段 1:
  支持 insert + search + range scan
  delete 使用 lazy delete 或 tombstone

阶段 2:
  支持 leaf compact
  支持空 leaf 回收

阶段 3:
  支持 borrow / merge
  支持 parent separator key 更新
  支持 root shrink
```

不要在第一次 B+Tree 实现里一次性完成所有删除平衡逻辑。

## 6. 推荐版本路线

```text
当前:
  catalog 持久化索引定义
  启动时从持久化 records rebuild 内存索引
  insert/update/delete 自动维护索引
  optimizer -> PhysicalIndexScan -> executor 查询索引
  HashIndex 提供等值查询
  MapIndex 作为 BTREE 兼容后端提供等值和范围查询

已完成:
  IndexManager 拆分为 IndexEngine + IndexStore
  一个 IndexStore 对应一个索引实例
  IndexStore 封装内存 Hash/Ordered 后端及单索引约束
  B+Tree leaf/internal 逻辑页与复合排序键
  BTreePageCodec 固定 4096 字节页格式
  BTreePageStore 文件头、PageId 分配和节点页持久化
  BTreeIndex 创建、打开和持有 BTreePageStore
  原 std::map 实现迁移为待废弃的 MapIndex

下一步:
  基于 BTreePageStore 实现 root-to-leaf 查找路径
  实现叶子页插入、按字节容量分裂和向上分裂传播
  让 BTreeIndex 实现 OrderedScalarIndex 并替换 IndexEngine 中的 MapIndex

后续:
  删除后的 borrow / merge / root shrink
  空闲页回收与崩溃一致性
  更完整的优化器和统计信息
```

## 7. 暂不实现

第一轮核心接入暂不实现：

- 唯一索引约束
- 多列联合索引
- 表达式索引
- 向量索引
- 持久化 Hash / B+Tree 文件
- page cache 中的索引页管理
- WAL / MVCC 下的索引一致性
- 成本优化器

这些功能应在标量索引基本生命周期稳定后再逐步加入。
