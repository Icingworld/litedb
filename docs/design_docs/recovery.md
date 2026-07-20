# LiteDB 事务与恢复机制

## 边界

当前事务层提供单写者、隐式语句级 `Serializable` DML。每条 `INSERT`、`UPDATE` 或 `DELETE` 是一个事务，Storage、持久化 B+Tree 和持久化 HNSW 共享同一 Commit Record。显式 `BEGIN/COMMIT`、DDL 事务、MVCC、LockManager、checkpoint 和 WAL 回收不在当前版本范围内。

## no-steal prepare

`TransactionContext` 保存 TransactionId、状态、LSN 和逻辑 row write set。Executor 必须先计算整条语句的所有目标行，再交给 `TransactionManager`。

第一版使用 `.transactions/txn_<id>/` staging 数据目录作为事务文件视图：复制现有 collection/index 文件，在隔离目录中复用当前 Storage、B+Tree 和 HNSW 算法，随后比较正式文件与 staging 文件，生成稳定 offset 的物理 after-image。提交前不会修改正式参与者文件。staging 是正确性优先的 MVP 后端，未来可以替换为内存 page overlay，而不改变 WAL 和恢复协议。

## WAL

WAL 位于 `wal/litedb.wal`，包含带版本的文件头以及 `Begin`、`FileWrite`、`Commit` record。每条 record 保存 LSN、TransactionId、payload 长度和 checksum。`FileWrite` 使用受控的 `FileKind + object_id + offset` 定位文件，不保存任意路径。

LSN 是 record 在 WAL 中的字节偏移。文件尾部不足一条完整 record 时启动过程截断尾部；完整 record checksum 错误时拒绝打开。

## Commit 协议

提交顺序固定为：

1. 在 staging 视图应用完整逻辑 write set；
2. 生成各参与者物理 after-image；
3. 追加 Begin、FileWrite 和 Commit；
4. 将 WAL 同步到 Commit LSN；
5. 将事务标记为 Committed；
6. 覆盖正式参与者文件；
7. 重新打开并原子发布 Storage、B+Tree 和 HNSW 运行时状态。

Commit WAL 同步前失败时丢弃 staging。Commit durable 后不允许 rollback；正式文件应用或运行时重新加载失败时，数据库进入 `RecoveryRequired` 并拒绝后续请求。

## 启动恢复

启动过程先打开 manifest 和 WAL，再加载原子 meta 快照。RecoveryManager 扫描 WAL、截断不完整尾部、识别具有 Commit Record 的事务，并按 LSN 顺序重放物理 after-image。无 Commit Record 的事务不会触及正式文件，因此直接忽略。

DDL 尚未写 WAL，所以恢复通过当前 catalog 判断目标是否仍然存活：后来被合法 DROP 的目标跳过；仍在 catalog 中但物理文件缺失则报告损坏。redo 完成并同步目标文件后，才解码和打开 Storage、B+Tree 与 HNSW。

redo 使用确定 offset 的覆盖写且不会向下 truncate，因此可以重复执行。WAL 当前不会回收，每次启动会重放全部 committed write；后续 checkpoint 必须在所有参与者文件同步后才能安全截断或轮换 WAL。
