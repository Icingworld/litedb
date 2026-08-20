# WAL 设计与实现

本文基于当前工作区中的实现，介绍 LiteDB WAL（Write-Ahead Log）的模块边界、磁盘格式、提交与恢复流程，以及 `WalManager` 和 `RecoveryManager` 的职责。阅读本文时，建议同时参考：

- `internal/src/core/wal/`
- `internal/src/core/transaction/transaction_manager.cpp`
- `internal/src/core/transaction/transaction_file_overlay.cpp`
- `internal/src/core/database/database_engine.cpp`

## 1. 先给出结论

LiteDB 当前实现的是一个**单机、单写者、语句级事务、物理 after-image、redo-only WAL**：

- 每条 DML 或 DDL 语句对应一个隐式事务。
- 事务先在内存中的稀疏文件覆盖层完成准备，不会提前修改正式数据文件。
- WAL 记录保存 collection、标量索引、向量索引和 catalog 文件的最终字节差异。
- WAL 中的 `Commit` 记录成功刷盘是事务的持久化提交点。
- 提交点之后才把字节差异应用到正式文件；正式文件此时不要求立即刷盘，因为持久化 WAL 可以在崩溃后重做。
- 恢复只重放具有完整 `Begin -> FileWrite* -> Commit` 序列的事务，不需要 undo。
- checkpoint 先强制同步所有正式数据文件和目录，再发布一个带 checkpoint 边界的新 WAL 代，最后删除旧代。

它不是 ARIES：没有 page LSN、prev-LSN 链、CLR、steal/no-steal 页面管理或 undo 阶段。它依靠“提交前不修改正式文件”和幂等 after-image redo，把恢复模型简化成 winner 重放。

## 2. 模块位置与依赖方向

`litedb_wal` 由以下组件组成：

| 组件 | 主要职责 |
| --- | --- |
| `WalCodec` | WAL v2 文件头、记录头和 `FileWrite` payload 的小端序编解码与 CRC32 校验 |
| `WalStore` | 单个 WAL 段文件的创建、打开、追加、刷盘、扫描和尾部截断 |
| `WalManager` | WAL 目录发现、活动段选择、写入预算校验、在线追加和 checkpoint 轮换 |
| `RecoveryManager` | 启动时校验事务序列、筛选已提交事务并 redo |
| `FileWriteBatch` | 文件写集合的规范化、冲突检查、目标路径解析与幂等应用 |
| `wal_types.hpp` | WAL 文件头、记录、文件目标、写入模式、扫描结果和资源限制 |
| `wal_error.hpp` | WAL 错误码和操作上下文 |

CMake 依赖方向是：

```text
filesystem + io
       |
       v
  litedb_wal
       |
       v
litedb_transaction
       |
       v
executor / database
```

WAL 不理解 SQL、行记录或索引语义。上层 `TransactionManager` 把各参与者的修改转换为 `FileWriteBatch`，WAL 只处理“哪个受支持的物理文件、哪个偏移、写入哪些最终字节”。

## 3. 整体架构

```text
Session / DatabaseEngine
          |
          | DML: Executor
          | DDL: CatalogEditor + DatabaseEngine
          v
   TransactionManager
          |
          | prepare
          v
TransactionFileOverlay  ----> FileWriteBatch
                                  |
                                  | Begin + FileWrite* + Commit
                                  v
                             WalManager
                                  |
                                  v
                              WalStore
                                  |
                                  v
                         <data>/wal/<generation>.wal

数据库重新打开：

WalManager::open -> RecoveryManager::recover -> FileWriteBatch::apply(sync=true)
                                           -> 再加载 catalog/storage/index/vindex
```

这里最重要的所有权边界是：

- storage/index/vindex/catalog 负责各自的文件格式和内存状态。
- `TransactionManager` 负责把多个参与者组织成一个原子语句级事务。
- WAL 负责提交记录先于正式数据持久化，以及崩溃后的 redo 能力。
- checkpoint 由 `TransactionManager` 协调，因为只有它知道所有持久化参与者。
- `DatabaseEngine` 负责启动顺序、SQL 串行化、自动 checkpoint 策略和观测指标。

## 4. WAL 记录的物理语义

### 4.1 `FileTarget`

WAL 不直接存储任意路径，而是记录受约束的 `(FileKind, object_id)`。恢复时再映射到数据目录内的固定位置：

| `FileKind` | `object_id` 约束 | 正式文件路径 |
| --- | ---: | --- |
| `CollectionStore` | 非 0 | `collections/<id>.store` |
| `ScalarIndex` | 非 0 | `indexes/<id>.bti` |
| `VectorIndex` | 非 0 | `vindexes/vindex_<id>.lhnsw` |
| `CatalogStore` | 必须为 0 | `catalog.lcat` |

这种设计避免把任意路径放入 WAL，也使恢复只能修改数据库目录中已知种类的文件。

### 4.2 `FileWriteMode`

| 模式 | 含义 | 幂等性来源 |
| --- | --- | --- |
| `Overwrite` | 在 `offset` 写入 `after_image`；文件不存在时可创建 | 重复写入相同最终字节 |
| `Replace` | 从偏移 0 写入完整 after-image，并截断为精确长度 | 重复完整替换得到相同文件 |
| `Delete` | 删除目标；目标不存在也视为成功 | 重复删除不改变结果 |
| `Truncate` | 把文件长度调整为 `offset` | 重复调整到同一长度 |

当前 `TransactionFileOverlay::export_batch()` 生成 `Overwrite`、`Truncate` 和 `Delete`；`Replace` 已被格式和应用层支持，但当前事务覆盖层不生成它。

### 4.3 `FileWriteBatch` 为什么重要

`FileWriteBatch` 不只是一个 vector。它在写入 WAL 前执行规范化：

1. 校验文件种类、对象 ID、模式、offset 和 after-image 组合。
2. 按文件目标和 offset 排序。
3. 合并同一文件中相邻的 `Overwrite`。
4. 拒绝重叠写入。
5. 同一目标最多允许一个 `Truncate`，并确保其他写入不超过最终长度。
6. `Delete` 或 `Replace` 必须是该目标唯一的生命周期操作。
7. 把 `Truncate` 排在该文件的普通写入之后。

恢复时 `apply()` 会再次规范化。这意味着即使 WAL 输入来自磁盘，应用层也不会盲目信任解码后的写集合。

`apply(data_directory, filesystem, sync)` 的行为是：

- 按目标逐个打开文件并执行操作。
- `sync=false` 时只应用字节，不强制正式文件落盘。
- `sync=true` 时在切换目标文件以及批次结束时同步当前文件。
- 删除操作不依赖目标必须存在。

正常提交使用 `sync=false`；启动恢复使用 `sync=true`。正常提交后由 WAL 提供持久性，checkpoint 再统一同步参与者。

## 5. WAL v2 磁盘格式

所有整数使用小端序。文件头和每条记录都带独立 CRC32；计算 CRC 时，checksum 字段先置零。

### 5.1 WAL 文件头：32 字节

| 偏移 | 大小 | 字段 | 当前约束 |
| ---: | ---: | --- | --- |
| 0 | 4 | magic | `0x4c57444c` |
| 4 | 2 | version | `2` |
| 6 | 2 | header size | `32` |
| 8 | 8 | generation | 非 0 |
| 16 | 8 | checkpoint transaction ID | 该代开始前已同步到正式文件的事务边界 |
| 24 | 4 | CRC32 | 整个 32 字节头，计算时本字段为 0 |
| 28 | 4 | reserved | 必须为 0 |

`generation` 同时编码在固定宽度文件名中，例如：

```text
<data>/wal/00000000000000000001.wal
<data>/wal/00000000000000000002.wal
```

文件名中的代号必须与文件头一致。

### 5.2 记录头：48 字节

| 偏移 | 大小 | 字段 | 当前约束 |
| ---: | ---: | --- | --- |
| 0 | 4 | magic | `0x3152574c` |
| 4 | 2 | version | `2` |
| 6 | 1 | type | `Begin=1`、`FileWrite=2`、`Commit=3` |
| 7 | 1 | flags | 必须为 0 |
| 8 | 8 | total size | `48 + payload_size` |
| 16 | 8 | LSN | 当前记录在该段文件中的起始偏移 |
| 24 | 8 | transaction ID | 非 0 |
| 32 | 8 | payload size | 必须等于 `total_size - 48` |
| 40 | 4 | CRC32 | 整条记录，计算时本字段为 0 |
| 44 | 4 | reserved | 必须为 0 |

当前 LSN 是**段内文件偏移**，不是跨 generation 单调递增的全局序号。轮换后第一条记录的 LSN 会重新从文件头后的偏移 32 开始。因此需要全局定位一条日志时，应把 `(generation, LSN)` 一起考虑。

`Begin` 和 `Commit` 的 payload 必须为空。一个事务的正常记录形态是：

```text
Begin(txn_id)
FileWrite(txn_id, write_1)
FileWrite(txn_id, write_2)
...
Commit(txn_id)
```

没有单独的 Abort Record。提交前失败的事务可以在 WAL 中留下 `Begin` 和若干 `FileWrite`，恢复时因缺少 `Commit` 而被忽略，之后由 checkpoint 一并回收。

### 5.3 `FileWrite` payload：24 字节固定头 + after-image

| 偏移 | 大小 | 字段 |
| ---: | ---: | --- |
| 0 | 1 | `FileKind` |
| 1 | 1 | `FileWriteMode` |
| 2 | 6 | reserved，必须为 0 |
| 8 | 8 | object ID |
| 16 | 8 | file offset；对 `Truncate` 表示最终文件长度 |
| 24 | 变长 | after-image |

after-image 长度由 WAL 记录的 payload 长度推导，不再重复存储。

## 6. `WalStore`：单段持久化机制

`WalStore` 是 `WalManager` 的下层机制，只负责一个已选择的 WAL 文件。

### 6.1 创建和打开

`create()`：

1. 编码并校验文件头参数。
2. 创建父目录。
3. 使用 `CreateNew`，拒绝覆盖已有 WAL。
4. 写入文件头并执行 `sync_all()`。
5. 写入或同步失败时关闭并尝试删除新文件。

`open()`：

1. 只打开已有文件，不隐式创建。
2. 检查文件至少有 32 字节。
3. 读取并校验 magic、version、header size、generation、reserved 和 CRC32。

### 6.2 追加与部分写失败

LSN 取追加前的真实文件长度。编码后的记录追加到文件尾；追加成功只表示进入文件系统，不表示已经持久化。

如果 append 报错，`WalStore` 会尝试：

1. 截断回追加前的旧长度。
2. 对截断结果执行 `sync_data()`。

只有这两步都成功，调用方才可以确认失败记录没有污染后续 WAL。如果截断或同步也失败，`WalStore` 设置 `recovery_required_`，拒绝后续修改。

### 6.3 刷盘

- `flush_through(lsn)` 检查 LSN 属于当前文件，然后执行 `sync_data()`；成功后记录最近确认的 LSN。
- `flush_all()` 执行 `sync_all()`，checkpoint 使用它同步活动 WAL。
- 任一刷盘失败都会把当前 store 标记为需要恢复，因为此时持久性结果不再可由进程内状态可靠判断。

### 6.4 扫描与尾部处理

扫描从偏移 32 开始：

1. 先读取 48 字节记录头。
2. 校验记录边界和预期 LSN。
3. 在分配完整记录缓冲区前检查资源限制。
4. 读取完整记录并校验 CRC32。
5. 更新有效尾部、最大事务 ID 和记录集合。

只有以下“物理不完整尾部”会被视为可截断：

- 剩余字节不足一个记录头。
- 记录声明的长度超过文件剩余长度。
- 完整记录读取发生短读。

已完整落入文件但 magic、字段或 CRC 错误的记录属于损坏，恢复会失败，不会把它当作普通 torn tail 静默丢弃。这避免错误地吞掉已持久化的结构性损坏。

## 7. 两个 manager 的职责

### 7.1 `WalManager`：在线 WAL 生命周期管理器

`WalManager` 是有状态对象，由 `DatabaseEngine` 持有，并被 `RecoveryManager` 和 `TransactionManager` 引用。它负责：

- 创建或发现 WAL 目录。
- 清理轮换遗留的 `*.wal.tmp`。
- 选择最高 generation 作为活动段。
- 持有活动 `WalStore`。
- 转发 `Begin`、`FileWrite`、`Commit` 追加和刷盘。
- 维护当前段记录数、大小、generation、checkpoint transaction ID 和保留段数指标。
- 在写入前校验事务是否仍能被配置的恢复预算完整扫描。
- checkpoint 时发布下一代 WAL 并清理旧代。

它**不负责**：

- 生成事务的文件写集合。
- 决定何时一个 SQL 事务开始或提交。
- 把已提交写集合应用到数据文件。
- 解释 collection/index/catalog 的内部格式。
- 在自身内部提供并发锁。

#### 打开活动段

`WalManager::open()` 的规则是：

1. 创建 WAL 目录并枚举文件。
2. 删除所有以 `.wal.tmp` 结尾的临时文件，并尽可能同步目录。
3. 只识别 20 位十进制 generation 加 `.wal` 的文件名。
4. 目录为空时创建 generation 1，并同步 WAL 目录。
5. 目录非空时扫描 generation 最大的段，截断不完整尾部并初始化记录计数。
6. 最高代损坏时直接失败，绝不退回较旧代。

不能回退到旧代非常重要：较高代一旦发布，就代表 checkpoint 后的正式文件已经成为恢复基线；回退旧代可能重放已被 checkpoint 覆盖的历史，甚至复活已删除对象。

#### 写入预算

`validate_transaction()` 在追加 `Begin/FileWrite*/Commit` 前检查：

- 每条 marker 是否能放入 `max_record_size_bytes`。
- 每条 `FileWrite` 的 48 字节记录头、24 字节 payload 头和 after-image 是否超限。
- 加入整个事务后记录总数是否超过 `max_record_count`。
- 加入整个事务后活动段大小是否超过 `max_scan_size_bytes`。

默认限制为：

- 单记录最大 512 MiB。
- 单段扫描最大 4 GiB。
- 单段最多 2,000,000 条记录。

这既是恢复时的防御性资源预算，也是写入时的 admission control：数据库不会主动写出一个按相同配置无法重新打开的事务。接近限制时需要先 checkpoint；当前实现不会在预算拒绝后自动 checkpoint 并重试事务。

### 7.2 `RecoveryManager`：启动 redo 协调器

`RecoveryManager` 是无状态工具类，只有静态 `recover()`。它在数据库打开阶段执行一次，不持有长期运行状态。

恢复分三步：

1. **扫描活动段**：调用 `wal.scan(true, limits)`，必要时截断并同步不完整尾部。
2. **验证事务语法**：对每个 transaction ID 检查：
   - 只能有一个 `Begin`。
   - `FileWrite` 必须位于 Begin 之后、Commit 之前。
   - 只能有一个 `Commit`。
   - Begin/Commit payload 必须为空。
3. **重放 winner**：只为存在 Commit 的事务解码 `FileWrite`，遇到该事务的 Commit 时调用 `FileWriteBatch::apply(..., sync=true)`。

没有 Commit 的 loser 事务完全忽略。恢复结果包含：

- `committed_transactions`：活动段中完整提交的事务数。
- `replayed_writes`：已提交事务中的 FileWrite 数。
- `maximum_transaction_id`：活动段所见最大 ID 与文件头 checkpoint ID 的最大值。

最大 ID 包含未提交事务的 ID，目的是重启后也不复用已经出现在 WAL 中的事务编号。`TransactionManager` 从这个最大值加一继续分配。

恢复不会自动清空 WAL。因此，同一活动段可能在多次重启时被重复重放；after-image 操作必须保持幂等。旧记录只在成功 checkpoint 后通过 segment 轮换回收。

### 7.3 两个 manager 的区别

| 维度 | `WalManager` | `RecoveryManager` |
| --- | --- | --- |
| 生命周期 | 数据库实例整个运行期 | 数据库打开时的一次调用 |
| 是否有状态 | 有：活动段、目录、记录数、限制和指标 | 无 |
| 主要方向 | 在线写入和段生命周期 | 从 WAL 到数据文件的 redo |
| 是否追加日志 | 是 | 否 |
| 是否修改正式数据文件 | 否 | 是，通过 `FileWriteBatch` |
| 是否理解事务完整性 | 只提供逐条追加和整批预算 | 校验 Begin/Write/Commit 并选择 winner |
| 是否负责 checkpoint | 负责 WAL 段发布部分 | 不负责 |
| 上层调用者 | `TransactionManager`、`DatabaseEngine` | `DatabaseEngine::initialize()` |

一句话概括：`WalManager` 管“日志现在写到哪里、如何安全换代”，`RecoveryManager` 管“启动时哪些日志算已提交、如何重做”。

## 8. 上层如何使用 WAL

### 8.1 数据库启动顺序

`DatabaseEngine::initialize()` 的关键顺序是：

1. 删除遗留的 `.transactions` 临时准备目录。
2. 初始化 database manifest。
3. `WalManager::open(<data>/wal, ...)`。
4. `RecoveryManager::recover(<data>, ..., wal)`。
5. 打开或初始化 `catalog.lcat`。
6. 根据恢复后的 catalog 打开 collection storage。
7. 恢复标量索引和向量索引运行时状态。
8. 用恢复得到的最大事务 ID 创建 `TransactionManager`。

WAL 恢复必须先于 catalog 和其他参与者的运行时加载，因为一个已提交 DDL 事务可能替换 catalog、创建或删除 collection/index 文件；先加载旧文件再 redo 会构造出过期的内存状态。

`.transactions` 只是事务准备时的临时命名空间，不是恢复来源。崩溃后的事实来源是已发布的 WAL。

### 8.2 DML 的调用路径

Executor 的 insert/update/delete 使用统一辅助流程：

```text
TransactionManager::begin_implicit()
    -> stage_insert / stage_update / stage_delete
    -> TransactionManager::commit()
```

`begin_implicit()` 获取 `TransactionManager::writer_mutex_`，并把锁的所有权移动到 `TransactionContext`。锁一直持有到 commit 或 abort，保证两个写事务不会交错追加 WAL 或应用文件。

### 8.3 DDL 的调用路径

DatabaseEngine 先通过 `CatalogEditor` 生成完整的新 catalog snapshot，然后：

```text
begin_implicit()
    -> stage_catalog(snapshot)
    -> commit()
```

准备阶段会在同一个覆盖层中同时产生 catalog、collection、标量索引和向量索引的文件变化。因此一次 DDL 的所有物理参与者共享同一条 Commit Record。

### 8.4 提交流程

一次 `TransactionManager::commit()` 的完整时序如下：

```text
上层                 TransactionManager       WalManager/WalStore       正式数据文件
 |                           |                         |                       |
 | stage mutations           |                         |                       |
 |-------------------------->|                         |                       |
 |                           | prepare in overlay      |                       |
 |                           | normalize batch         |                       |
 |                           | validate WAL budget     |                       |
 |                           | append Begin            |                       |
 |                           |------------------------>|                       |
 |                           | append FileWrite*       |                       |
 |                           |------------------------>|                       |
 |                           | append Commit           |                       |
 |                           |------------------------>|                       |
 |                           | flush_through(Commit)   |                       |
 |                           |------------------------>| sync_data             |
 |                           |                         |                       |
 |                           | <== 持久化提交点 ==>    |                       |
 |                           |                         |                       |
 |                           | apply(sync=false)       |                       |
 |                           |----------------------------------------------->|
 |                           | publish catalog/reload runtime                  |
 |                           |                       <-------------------------|
 | commit success            |                         |                       |
 |<--------------------------|                         |                       |
```

准备阶段的细节是：

1. `TransactionFileOverlay` 以正式数据目录为只读基线。
2. storage/index/vindex/catalog 仍通过各自正常文件 API 工作，但写入被覆盖层截获到 4 KiB dirty block。
3. `export_batch()` 比较 dirty block 与正式文件，只导出真正变化的连续 after-image，并在需要时附加 `Truncate` 或 `Delete`。
4. `FileWriteBatch::normalize()` 在任何日志持久化前拒绝冲突写集合。

提交过程有两个不同的时间点：

- **持久化提交点**：Commit Record 成功 `flush_through()`。从此事务不能再回滚。
- **调用成功返回点**：正式文件应用成功，catalog 和运行时引擎重新加载成功之后。

因此，如果 Commit 已刷盘但后续 apply/reload 失败，调用可能返回错误，但事务已经持久提交；数据库会进入 `recovery_required`，重启后 redo。调用方不能把这种错误简单理解成“事务一定没有提交”。

### 8.5 为什么提交后应用正式文件时不刷盘

正常提交调用 `FileWriteBatch::apply(..., sync=false)`。这是有意的：

```text
先持久化 WAL Commit
        |
        v
正式文件可以只进入页缓存
        |
        +-- 正常运行：内存状态重新加载并继续服务
        |
        +-- 此时崩溃：启动恢复从 WAL 重做
```

这样避免每个事务同时同步 WAL 和所有参与者文件。代价是 checkpoint 之前不能丢弃 WAL。

## 9. Checkpoint 与 WAL 轮换

### 9.1 checkpoint 的上层阶段

`TransactionManager::checkpoint()` 获取与写事务相同的 `writer_mutex_`，确保 checkpoint 位于事务边界上。它执行：

1. 计算 checkpoint transaction ID：当前已分配最大事务 ID。
2. `wal.flush_all()`。
3. 对向量索引执行 checkpoint/压缩。
4. 同步 `catalog.lcat` 和 catalog 当前引用的所有 collection/index/vindex 文件。
5. 同步 `collections`、`indexes`、`vindexes` 和数据根目录；平台不支持目录同步时保留 best-effort 行为。
6. 调用 `WalManager::rotate(checkpoint_transaction_id)`。

前五步保证：一旦新 WAL 代被发布，旧 WAL 所代表的已提交状态已经存在于正式参与者文件中。

### 9.2 `WalManager::rotate()` 的发布协议

假设当前 generation 为 N：

1. 创建 `generation N+1` 的 `.wal.tmp`。
2. 新文件头写入 `generation=N+1` 和 checkpoint transaction ID。
3. 对临时 WAL 执行完整同步。
4. rename 为最终 `.wal` 文件。
5. 同步 WAL 目录。
6. 打开新段并切换为 active。
7. 删除所有更旧 generation。
8. 再次同步 WAL 目录。

```text
同步所有参与者
      |
      v
new.wal.tmp --sync--> new.wal --sync directory--> active=new
                                                     |
                                                     v
                                               remove old WALs
                                                     |
                                                     v
                                               sync directory
```

崩溃选择规则：

- 新代发布前崩溃：启动时删除 `.wal.tmp`，继续从旧代恢复。
- 新代发布后崩溃：启动时选择最高代；其 checkpoint header 说明旧事务已进入正式文件，所以不再扫描旧代。
- 旧段尚未删除时崩溃：仍选择最高代，旧段只是待回收文件。

### 9.3 手动与自动 checkpoint

- 手动：`DatabaseEngine::checkpoint()` 在数据库级 mutex 下同步执行。
- 自动：`DatabaseConfig::automatic_checkpoint.wal_size_threshold_bytes` 非 0 时，每条成功写 WAL 的 DML/DDL 语句之后检查活动 WAL 大小。
- 默认阈值是 0，即关闭自动 checkpoint。

自动 checkpoint 发生在触发它的语句已经持久提交之后。如果维护操作失败，DatabaseEngine 只增加失败指标，不把错误返回成该语句失败，避免调用方重试一个实际上已经提交的语句。错误是否会阻止后续请求，取决于失败是否使 `TransactionManager` 或 WAL 进入 `recovery_required`。

## 10. 崩溃与错误语义

| 失败位置 | WAL/数据状态 | 重启后的结果 |
| --- | --- | --- |
| prepare 期间 | 正式文件未改，可能只有临时覆盖层 | 清理 `.transactions`，事务消失 |
| Begin 或 FileWrite 后、Commit 前 | WAL 中是 loser，正式文件未改 | 忽略该事务 |
| 记录只写入部分尾部 | 最后一条记录物理不完整 | 截断到最后有效边界并同步 |
| 完整记录 CRC 或结构损坏 | WAL 内容不可可信 | 打开失败，不静默截断 |
| Commit 已 append、尚未确认 flush | 持久结果不确定 | 当前进程停止写入；重启后以实际扫描到的完整 Commit 为准 |
| Commit 已成功 flush，apply 前崩溃 | winner 在 WAL，正式文件可能仍旧 | redo 全部 FileWrite |
| apply 中途崩溃 | 多个参与者可能只应用一部分 | 幂等 redo 整个已提交批次 |
| apply 完成但正式文件未落盘 | WAL 仍保留 | redo 丢失的字节 |
| checkpoint 新代发布前崩溃 | 旧代仍是最高有效代 | 从旧代恢复 |
| checkpoint 新代发布后崩溃 | 新代 header 是新恢复基线 | 选择新代，不重放旧代 |
| 最高 generation 损坏 | 不能安全判断 checkpoint 基线 | 失败，不回退旧代 |

### `recovery_required` 的含义

以下情况会进入保守的恢复要求状态：

- WAL append 失败后无法确认截断回旧尾部。
- WAL flush 失败，无法确认 Commit 是否持久。
- Commit 进入可能持久的阶段后，正式文件 apply 或运行时 reload 失败。
- WAL 轮换中出现可能影响发布状态的错误。

进入该状态后，当前 `DatabaseEngine` 拒绝新事务。当前实现没有在线清除该状态的 API；需要关闭并重新打开数据库，让启动扫描和 redo 根据磁盘事实重新建立状态。

## 11. 并发与可见性边界

WAL 类本身没有内部互斥保护，其正确性依赖上层串行化：

- `Session::execute_sql()` 持有 `DatabaseEngine::mutex_`，当前 SQL 流水线整体串行化。
- `TransactionManager::begin_implicit()` 获取单写者锁，并由 `TransactionContext` 持有到 commit/abort。
- `TransactionManager::checkpoint()` 获取同一单写者锁，确保不会在事务记录中间换代。

因此当前实现不支持多个并发 writer，也没有 group commit。每个成功写事务都会单独同步包含 Commit 的 WAL。

如果绕过 DatabaseEngine 直接使用 `WalManager`，调用方必须自行满足：

- 打开后先扫描/恢复，再开始追加。
- 串行化 append、scan 和 rotate。
- 不在一个事务的 Begin 与 Commit 之间轮换段。
- 只在所有 checkpoint 参与者已经可靠同步后发布新 generation。

## 12. 配置、指标与错误

### 12.1 配置

`DatabaseConfig` 暴露：

- `wal_decode_limits`：扫描和写入 admission 共用的记录大小、段大小和记录数预算。
- `automatic_checkpoint.wal_size_threshold_bytes`：按活动 WAL 大小触发自动 checkpoint；0 表示关闭。

### 12.2 观测指标

`DatabaseEngine::observability()` 可以看到：

- 启动时发现的已提交事务数和实际 redo 写入数。
- WAL 当前大小、generation 和 checkpoint transaction ID。
- 事务开始、提交、中止、失败和提交耗时。
- checkpoint 成功/失败、耗时和回收的 WAL 字节。
- 自动 checkpoint 尝试、成功和失败次数。

`wal_size_bytes` 是当前活动段的缓存大小，不是整个 WAL 目录所有残留文件的总大小。

### 12.3 错误分类

WAL 自有错误包括：格式/版本错误、记录损坏、非法记录、未知目标、资源超限、需要恢复和 apply 中断。底层 filesystem/io 错误通常保留原始错误类别向上传递。`WalErrorContext` 可以附带操作、路径、事务 ID、LSN 和 generation，便于定位失败阶段。

## 13. 当前保证与明确限制

当前设计可以提供：

- 单写者边界内，catalog、collection、标量索引和向量索引共享一个事务提交点。
- Commit 先于正式文件持久化。
- 未提交事务不进入正式文件，也不会在恢复中重放。
- 已提交事务即使只应用部分参与者，也能在重启后完成幂等 redo。
- torn tail 可恢复，完整记录损坏会显式报错。
- checkpoint 发布在参与者强制同步之后，并通过 generation 选择唯一恢复基线。

当前没有提供：

- 显式多语句事务或 savepoint。
- 多写者并发、MVCC 或 group commit。
- undo、逻辑日志或按页恢复。
- WAL 压缩、归档、复制或 point-in-time recovery。
- 跨 generation 的全局 LSN。
- 最高代损坏时的自动回退或人工修复策略。
- 在线 recovery；`recovery_required` 后需要重新打开数据库。
- 磁盘格式向后兼容承诺。

## 14. 建议的代码阅读顺序

如果继续 review WAL，推荐按下面顺序阅读：

1. `wal_types.hpp`：先理解 WAL 存的不是行，而是受约束的文件 after-image。
2. `file_write_batch.cpp`：确认幂等 redo、冲突规则和物理目标映射。
3. `wal_codec.cpp`：检查 v2 格式、大小关系、CRC 和解码预算。
4. `wal_store.cpp`：检查单段 append/flush/scan/torn-tail 语义。
5. `wal_manager.cpp`：检查最高代选择、写预算和轮换发布协议。
6. `recovery_manager.cpp`：检查事务序列验证、winner/loser 和重放顺序。
7. `transaction_file_overlay.cpp`：理解上层如何生成 4 KiB 差异。
8. `transaction_manager.cpp`：核对提交点、apply、reload、checkpoint 和故障状态。
9. `database_engine.cpp` 与 `session.cpp`：核对启动顺序、SQL 串行化和自动 checkpoint。

review 时最值得持续追问的三个问题是：

1. 某个错误发生时，Commit 是否可能已经持久化？
2. 如果正式参与者只完成一部分写入，重启 redo 是否仍然幂等并覆盖全部参与者？
3. 旧 WAL 被删除前，所有由它保护的正式文件和目录是否已经可靠同步？

## 15. 相关测试

| 测试 | 重点覆盖 |
| --- | --- |
| `wal_tests` | v2 golden bytes、编解码、扫描预算、batch 规范化、生命周期操作、尾部截断、轮换和最高代损坏 |
| `wal_fault_injection_tests` | append 部分失败回滚、truncate/sync/flush 不确定性和 recovery-required 传播 |
| `transaction_file_overlay_tests` | 稀疏 4 KiB 覆盖、extend/truncate、创建删除与差异导出 |
| `transaction_recovery_tests` | 多参与者 redo、loser 忽略和重复恢复幂等性 |
| `transaction_crash_tests` | DML 提交各阶段崩溃 |
| `ddl_crash_tests` | DDL/catalog 与物理对象发布各阶段崩溃 |
| `checkpoint_tests` | 手动/自动 checkpoint、资源预算和事务 ID 单调性 |
| `checkpoint_crash_tests` | 新 WAL 代发布前后的 generation 选择与恢复基线 |

聚焦验证命令：

```sh
cmake --build build --target wal_tests wal_fault_injection_tests transaction_file_overlay_tests transaction_recovery_tests transaction_crash_tests ddl_crash_tests checkpoint_tests checkpoint_crash_tests
ctest --test-dir build -R "^(wal_tests|wal_fault_injection_tests|transaction_file_overlay_tests|transaction_recovery_tests|transaction_crash_tests|ddl_crash_tests|checkpoint_tests|checkpoint_crash_tests)$" --output-on-failure
```
