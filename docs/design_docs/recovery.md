# LiteDB 事务与恢复机制

## 边界

当前事务层提供单写者、隐式语句级 `Serializable` 事务。每条 DML 或 DDL 语句都是一个事务；Meta、Storage、持久化 B+Tree 和持久化 HNSW 共享同一 Commit Record。显式 `BEGIN/COMMIT`、MVCC 和 LockManager 不在当前版本范围内。当前提供同步手动 checkpoint，尚无后台或阈值自动 checkpoint。

## no-steal prepare

`TransactionContext` 保存 TransactionId、状态、LSN、逻辑 row write set，以及可选的提交后 MetaSnapshot。Executor 必须先计算整条 DML 的所有目标行；DatabaseEngine 则先在无持久化的 MetaEngine 中生成 DDL 提交后快照，再交给 `TransactionManager`。

第一版使用 `.transactions/txn_<id>/` staging 数据目录作为事务文件视图：复制现有 collection/index 文件，在隔离目录中复用当前 Storage、B+Tree 和 HNSW 算法，随后比较正式文件与 staging 文件，生成稳定 offset 的物理 after-image。提交前不会修改正式参与者文件。staging 是正确性优先的 MVP 后端，未来可以替换为内存 page overlay，而不改变 WAL 和恢复协议。

DDL 在 staging 中持久化新的 MetaSnapshot，并按提交后 catalog 创建或验证 collection、B+Tree 与 HNSW 文件。catalog 中消失的对象生成幂等 Delete 操作；新增或变化的对象生成完整 Replace after-image。

## WAL

WAL 使用按 generation 命名的 `wal/<20-digit-generation>.wal` 分段。文件头保存格式版本、generation、已 checkpoint 的最大 TransactionId 和 checksum；段内包含 `Begin`、`FileWrite`、`Commit` record。每条 record 保存段内 LSN、TransactionId、payload 长度和 checksum。`FileWrite` 使用受控的 `FileKind + object_id + offset` 定位文件，不保存任意路径；其模式包括范围 Overwrite、完整 Replace 和幂等 Delete，MetaStore 也是受控 FileKind。

LSN 是 record 在当前 WAL 段中的字节偏移；第一版不允许事务跨段。文件尾部不足一条完整 record 时启动过程截断尾部；完整 record checksum 错误时拒绝打开。当前格式直接升级，不识别早期单文件 WAL。

## Checkpoint 与轮换

`DatabaseEngine::checkpoint()` 在 `TransactionManager` 的全局 writer guard 下同步执行，因此 checkpoint 期间不会有活跃写事务。顺序固定为：

1. 同步当前 WAL；
2. 对当前 catalog 可达的 Meta、collection、B+Tree 和 HNSW 文件执行 `sync_all`；
3. 同步数据根目录以及 `collections`、`indexes`、`vindexes` 目录；
4. 写入并同步下一 generation 的临时空 WAL，文件头记录 checkpoint TransactionId；
5. 将临时段 rename 为正式段并同步 WAL 目录；
6. 切换内存 active segment；
7. 清理旧 generation 并再次同步 WAL 目录。

旧 WAL 不会原地 truncate。发布新正式段之前崩溃时，启动继续选择旧段并 redo；发布之后崩溃时，启动选择 generation 最大的正式段，参与者文件此时已经持久化。`.wal.tmp` 从不具备权威性，启动会清理。若最高 generation 的正式段损坏，启动直接失败，不回退到旧段。

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

启动过程先打开 manifest，再从 WAL 目录选择 generation 最大的正式段。RecoveryManager 在加载 Meta 之前扫描该段、截断不完整尾部、识别具有 Commit Record 的事务，并按 LSN 顺序重放 Meta 与物理文件操作；随后才加载恢复后的 MetaSnapshot 并打开 Storage、B+Tree 与 HNSW。无 Commit Record 的事务不会触及正式文件，因此直接忽略。新段文件头中的 checkpoint TransactionId 同样参与下一个 TransactionId 的计算，保证轮换后 ID 单调递增。

Replace、Delete 和确定 offset 的 Overwrite 都是幂等操作，因此可以重复执行。CREATE 的物理文件先于 Meta 发布，DROP 的 Meta 更新先于物理删除；即使恢复过程再次崩溃，下次启动仍会从当前 active WAL 重放到同一状态。
