# LiteDB 事务与恢复机制

## 边界

当前事务层提供单写者、隐式语句级 `Serializable` 事务。每条 DML 或 DDL 语句都是一个事务；Meta、Storage、持久化 B+Tree 和持久化 HNSW 共享同一 Commit Record。显式 `BEGIN/COMMIT`、MVCC、LockManager、checkpoint 和 WAL 回收不在当前版本范围内。

## no-steal prepare

`TransactionContext` 保存 TransactionId、状态、LSN、逻辑 row write set，以及可选的提交后 MetaSnapshot。Executor 必须先计算整条 DML 的所有目标行；DatabaseEngine 则先在无持久化的 MetaEngine 中生成 DDL 提交后快照，再交给 `TransactionManager`。

第一版使用 `.transactions/txn_<id>/` staging 数据目录作为事务文件视图：复制现有 collection/index 文件，在隔离目录中复用当前 Storage、B+Tree 和 HNSW 算法，随后比较正式文件与 staging 文件，生成稳定 offset 的物理 after-image。提交前不会修改正式参与者文件。staging 是正确性优先的 MVP 后端，未来可以替换为内存 page overlay，而不改变 WAL 和恢复协议。

DDL 在 staging 中持久化新的 MetaSnapshot，并按提交后 catalog 创建或验证 collection、B+Tree 与 HNSW 文件。catalog 中消失的对象生成幂等 Delete 操作；新增或变化的对象生成完整 Replace after-image。

## WAL

WAL 位于 `wal/litedb.wal`，包含带版本的文件头以及 `Begin`、`FileWrite`、`Commit` record。每条 record 保存 LSN、TransactionId、payload 长度和 checksum。`FileWrite` 使用受控的 `FileKind + object_id + offset` 定位文件，不保存任意路径；其模式包括范围 Overwrite、完整 Replace 和幂等 Delete，MetaStore 也是受控 FileKind。

LSN 是 record 在 WAL 中的字节偏移。文件尾部不足一条完整 record 时启动过程截断尾部；完整 record checksum 错误时拒绝打开。

## Commit 协议

提交顺序固定为：

1. 在 staging 视图应用完整逻辑 write set；
2. 生成各参与者物理 after-image；
3. 追加 Begin、FileWrite 和 Commit；
4. 将 WAL 同步到 Commit LSN；
5. 将事务标记为 Committed；
6. 覆盖正式参与者文件；
7. 发布 Meta，并重新打开 Storage、B+Tree 和 HNSW 运行时状态。

Commit WAL 同步前失败时丢弃 staging。Commit durable 后不允许 rollback；正式文件应用或运行时重新加载失败时，数据库进入 `RecoveryRequired` 并拒绝后续请求。

## 启动恢复

启动过程先打开 manifest 和 WAL。RecoveryManager 在加载 Meta 之前扫描 WAL、截断不完整尾部、识别具有 Commit Record 的事务，并按 LSN 顺序重放 Meta 与物理文件操作；随后才加载恢复后的 MetaSnapshot 并打开 Storage、B+Tree 与 HNSW。无 Commit Record 的事务不会触及正式文件，因此直接忽略。

Replace、Delete 和确定 offset 的 Overwrite 都是幂等操作，因此可以重复执行。CREATE 的物理文件先于 Meta 发布，DROP 的 Meta 更新先于物理删除；即使恢复过程再次崩溃，下次启动仍会从 WAL 重放到同一状态。WAL 当前不会回收，每次启动会重放全部 committed write；后续 checkpoint 必须在所有参与者文件同步后才能安全截断或轮换 WAL。
