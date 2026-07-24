# LiteDB Storage v2

## 边界与兼容性

Storage v2 由 `StorageEngine`、每 collection 一个的 `StorageStore` 和拥有数据的
`StorageCursor` 组成。文件位于 `collections/<collection_id>.store`。

这是不兼容的直接升级：

- database manifest format version 和 collection Store format version 均为 `2`；
- v1 manifest、旧 `.store` 和更早的 `.rows` 文件返回 `UnsupportedVersion` 或数据库格式错误；
- 不尝试迁移、原地重写或猜测旧格式。

`StorageStore` 不实现独立 WAL、journal 或 double-write。正式文件的 SQL 原子性与
durability 由 `TransactionManager`、redo WAL 和 checkpoint 提供。

## 运行时生命周期

`StorageEngine` 有两种打开模式：

- `LiveReadOnly`：允许 open、reload、get 和 scan；
- `TransactionalStaging`：额外允许 create、drop、insert、update 和 erase。

live engine 的所有 DML/DDL 都稳定返回 `StorageErrorCode::InvalidState`。事务准备阶段在
`TransactionFileOverlay` 上创建 staging engine，提交成功并应用正式文件后才 reload
live runtime。

`scan()` 每个数据页只读取和校验一次，并生成拥有完整 `Record` 的 snapshot cursor。
Cursor 不保存 `StorageStore*`，因此 engine reload、drop 或 clear 后仍可安全消费。

## 通用编码

- byte order：little-endian；
- page size：4096 bytes；
- 浮点：IEEE-754；
- Value tag：保持 `0..7`，分别表示 NULL、BOOL、INT32、INT64、FLOAT、DOUBLE、
  VARCHAR 和 VECTOR；
- Record payload：`record_id:u64 + value_count:u32 + values`。

单条编码记录必须完整消费其 slot bytes；任何尾随 bytes 都是 `InvalidFormat`。记录无法放入
一个空数据页时返回 `RecordTooLarge`，当前没有 overflow page。

## 文件头页

文件头固定占一个 4096-byte page：

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | magic `LDS2` |
| 4 | 2 | format version = 2 |
| 6 | 2 | header size = 4096 |
| 8 | 4 | page size = 4096 |
| 12 | 4 | flags = 0 |
| 16 | 8 | collection ID |
| 24 | 8 | next RecordId |
| 32 | 4 | data page count |
| 36 | 4 | CRC32 |
| 40 | 4056 | reserved = 0 |

CRC32 复用 `core/io/checksum.hpp`；计算时 checksum 字段按零处理。打开时校验 magic、
version、header/page size、flags、reserved、collection ID、RecordId 计数器、文件长度、
page count 和 CRC32。

## 数据页

数据页头固定 24 bytes：

| Offset | Size | Field |
|---:|---:|---|
| 0 | 4 | magic `LPG2` |
| 4 | 4 | page ID |
| 8 | 2 | slot count |
| 10 | 2 | free start |
| 12 | 2 | free end |
| 14 | 2 | flags = 0 |
| 16 | 4 | CRC32 |
| 20 | 4 | page generation |

slot 固定 8 bytes：

| Offset | Size | Field |
|---:|---:|---|
| 0 | 2 | payload offset |
| 2 | 2 | payload length |
| 4 | 1 | state: active=1, deleted=2 |
| 5 | 3 | reserved = 0 |

解码拒绝非法 flags/state、非零 reserved、错误 free bounds、越界或互相重叠的 payload、
错误 page ID、CRC32 不匹配、截断页、duplicate RecordId，以及小于最大已有 ID 的
`next_record_id`。

## 页管理与空间复用

打开 collection 时建立按“可回收空间、page ID”排序的 free-space index。

插入采用 best-fit 候选顺序：

1. 连续空间足够时直接写入；
2. 总可回收空间足够但连续空间不足时 compact；
3. 所有现有页都不足时追加新页。

compact 将 active payload 重新排列到页尾，清零废弃区域，更新所有 slot offset 和 free
bounds，递增 generation，并重新计算 CRC32。deleted slot 被复用，空页保留供后续插入，
当前不在线截断 collection 文件。

update 先在事务 overlay 中移除旧 payload，再 compact 并尝试原页 reinsert；原页不足时才
迁移。delete 立即更新 free-space index。`RecordId` 到物理位置使用有序映射，scan 不再为
排序复制位置目录。

## 错误与指标

Storage 使用统一 `error::Error`，category 为 `ErrorCategory::Storage`。稳定的
`StorageErrorCode : uint8_t` 覆盖 collection/record、schema、filesystem、IO、EOF、
format、checksum、resource limit、invalid state 和 durability unknown。

`StorageErrorContext` 保存 operation、path、collection ID、record ID、page ID、slot ID
和下层 encoded source code；filesystem/IO 错误在 Storage 边界转换时保留结构化来源。

只读 `StorageMetrics` 提供 page reads/writes、bytes read/written、compactions、
reused/new pages 和 checksum failures。

## 事务与当前限制

正常 SQL DML/DDL 必须经过 `TransactionManager`。prepare 只修改 4 KiB block 粒度的
稀疏 overlay；Commit Record durable 后，幂等应用 Overwrite、Truncate 和 Delete，再
reload runtime。恢复必须在加载 Storage 前完成。

当前仍不提供：

- MVCC、并发写者或显式多语句事务；
- overflow page；
- 后台 vacuum、在线文件截断或后台 compaction；
- `StorageStore` 自身的 crash atomicity。
