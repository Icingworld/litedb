# LiteDB Storage v2

## 边界

Storage v2 由 `StorageEngine`、内部 `Store` 和 `StorageCursor` 组成。Executor、Index 和其他上层模块只能按 `CollectionId` 调用 `StorageEngine`，不能取得 collection Store 指针。

当前同时支持内存 Store 和文件 Store。文件模式为每个 collection 创建独立的 `collections/<collection_id>.store`。旧 `.rows`/RowLog 格式不再支持，也不提供迁移。

文件 Store 仍不直接实现事务；DML 由上层 `TransactionManager` 以隐式语句级事务协调，使用 redo-only WAL 和 no-steal staging 文件视图覆盖 Storage、B+Tree 与 HNSW。当前不提供显式 SQL 事务、MVCC、buffer pool、checkpoint、WAL 回收或并发写保证。

## 标识

- 对外使用单调递增且不复用的逻辑 `RecordId`。
- 文件 Store 内部使用 `PhysicalRid { page_id, slot_id }` 定位记录。
- Store 打开时扫描所有活动 slot，重建 `RecordId -> PhysicalRid` 内存映射。
- update 可搬迁物理记录，但保持逻辑 RecordId 不变。

## 文件头

文件头占用一个 4096-byte page，包含：

- magic `LDS2`；
- format version；
- header size；
- page size，固定为 4096；
- collection ID；
- next record ID；
- data page count。

打开 Store 时必须校验 magic、版本、page size、collection ID、文件长度和计数器。

## 数据页

数据页采用 slotted-page：

```text
+---------------- page header ----------------+
| magic | page_id | slot_count | free bounds  |
+---------------- slot directory -------------+
| offset | length | state | ...                |
+---------------- free space -----------------+
|                                               |
+---------------- record payloads ------------+
```

活动 record payload 保存逻辑 RecordId、value count 和按 schema 顺序编码的 Value。slot 状态当前为 active 或 deleted。

- insert 优先使用有足够连续空间的页和已删除 slot，否则追加新页；
- update 写入新 payload 后删除旧 slot，并更新内存位置映射；
- erase 标记 slot deleted；
- scan 顺序读取逻辑 RecordId 列表，并在 `next()` 时从 Store 解码记录；
- 单条编码记录无法放入空页时返回 `RecordTooLarge`；暂不支持 overflow page。

## 错误与耐久性

Store 层用 `StoreError` 描述文件系统、IO、格式、版本、损坏页和物理状态错误。StorageEngine 将其包装为公开 `StorageError`，同时保留可选 `store_code`。

StorageEngine 统一执行 value count、logical type、NULL、VARCHAR 长度和 VECTOR 维度校验。Store 只接收已经校验的记录。

Store primitive 本身仍不主动 sync，也不单独提供原子性。正常 SQL DML 必须经过 `TransactionManager`：提交前只修改 staging 副本，物理 after-image 与 Commit Record 写入并同步 WAL 后，才覆盖正式文件并重新发布运行时状态。启动时只重放具有 Commit Record 且仍存在于 catalog 的目标；未提交事务被忽略。

DDL 暂不进入 WAL。当前 meta 仍使用自己的原子快照提交协议，事务恢复会根据已加载 catalog 跳过后来被合法删除的 collection/index 目标。完整协议见 `recovery.md`。
