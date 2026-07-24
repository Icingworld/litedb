# LiteDB 事务与恢复机制

## 边界

当前提供全局单写者、隐式语句级 `Serializable` 事务。每条 DML 或 DDL 都是一个事务；
Meta、Storage、持久化 B+Tree 和持久化 HNSW 共享同一 Commit Record。尚无显式
`BEGIN/COMMIT/ROLLBACK`、MVCC、LockManager 或后台 checkpoint。

## 稀疏 no-steal prepare

`TransactionContext` 保存 TransactionId、状态、LSN、row write set 和可选的提交后
MetaSnapshot。prepare 使用 `TransactionFileOverlay` 包装正式 `FileSystemBackend`：

- 逻辑路径仍位于 `.transactions/txn_<id>/`，但正常 prepare 不创建或复制整目录；
- 以 4096-byte block 按需读取正式文件；
- 只保存 dirty block、逻辑文件长度以及 create/delete 状态；
- 支持 read-after-write、跨 block 写、extend、truncate、临时文件、rename、
  atomic replace 和虚拟 sync；
- Meta、Storage、B+Tree 与 HNSW 继续使用原有文件 API。

旧 `.transactions/txn_*` 目录仍会在启动时清理一个兼容周期，但新 prepare 不依赖这些
物理副本。

prepare 完成后，overlay 导出最终 `FileWriteBatch`：

- 与正式文件内容相同的 dirty block 被消除；
- 相邻 dirty block 合并为一个 Overwrite；
- 最终逻辑长度用 Truncate 表达；
- 新文件通过 Overwrite 自动创建和扩展；
- 删除使用幂等 Delete。

batch 按 target/offset 规范化，拒绝非法 target、offset overflow、重叠范围、冲突的
Replace/Delete/Truncate。相同 target 的范围写入共用一个 handle，Truncate 最后执行，
每个 target 统一 sync。

## WAL

WAL 使用 `wal/<20-digit-generation>.wal`。文件头保存 format version、generation、
checkpoint TransactionId 和 checksum。段内记录为 Begin、FileWrite 和 Commit，每条
记录保存 LSN、TransactionId、payload length 和 checksum。

`FileWrite` 使用受控的 `FileKind + object_id + offset`，不记录任意路径。模式包括：

- `Overwrite`：offset 处的 redo after-image；
- `Replace`：兼容完整文件替换记录；
- `Delete`：幂等删除；
- `Truncate`：将最终文件长度设为 offset。

正常单行 DML 的 WAL 与实际 dirty block 数量相关，不再复制 collection 或 index 全文件。
WAL 尾部不足完整 record 时启动截断尾部；完整 record checksum 错误时拒绝打开。
扫描与恢复使用可配置的 `WalDecodeLimits`，默认限制单 record 为 512 MiB、active WAL
为 4 GiB、record 数量为 2,000,000。超过预算返回 `ResourceLimitExceeded`，不会按磁盘长度
直接执行无界内存分配；确有更大事务或恢复窗口的嵌入方可以通过 `DatabaseConfig` 显式放宽。

`WalError` 使用统一的 move-only `error::Error`。`WalErrorCode` 保留 WAL 领域分类，
`WalErrorContext` 携带 operation、path、TransactionId、LSN、generation 和下层 encoded
error code；跨层传播错误时不得退回仅保留字符串的旧结构。

## Commit 协议

顺序固定为：

1. 在 overlay filesystem 上应用完整逻辑 write set；
2. 导出并规范化物理 after-image batch；
3. 追加 Begin、所有 FileWrite、Commit；
4. 将 WAL flush 到 Commit LSN；
5. 将事务标记为 Committed；
6. 将 batch 应用到正式 Meta/Storage/B+Tree/HNSW 文件；
7. DML reload 受影响 collection；DDL 发布 Meta 后全量恢复 runtime。

Commit WAL flush 前失败时丢弃 overlay。Commit durable 后不允许 rollback；正式文件 apply
或 runtime reload 失败时进入 `RecoveryRequired`，拒绝后续请求。

## 启动恢复

启动先打开 manifest，再选择 generation 最大的正式 WAL 段。RecoveryManager 在加载 Meta
和 Storage 前扫描 WAL，识别具有 Commit Record 的事务，并按 LSN 重放全部 FileWrite。
无 Commit Record 的事务不会修改正式文件，直接忽略。

Overwrite、Replace、Truncate 和 Delete 都按 redo after-image 语义幂等执行。若恢复再次
崩溃，下次启动从同一 active WAL 重新重放，最终状态只能是提交前或提交后，不能发布
半提交的 Storage/Meta/Index 组合。

## Checkpoint

`DatabaseEngine::checkpoint()` 在全局 writer guard 下同步执行：

1. sync 当前 WAL；
2. sync 当前 catalog 可达的 Meta、collection、B+Tree 和 HNSW 文件；
3. sync 数据目录；
4. 写入并 sync 下一 generation 的临时空 WAL；
5. rename 发布新段并 sync WAL 目录；
6. 切换 active segment；
7. 清理旧 generation。

可选 WAL-size threshold 可在成功写语句后触发同步 checkpoint。自动 checkpoint 失败不会
把已经 durable 的语句改报为失败；不确定的轮换结果仍进入 `RecoveryRequired`。
