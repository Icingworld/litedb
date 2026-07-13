# litedb 项目路线图

## 1. 项目定位

litedb 的长期定位是一个 CS 架构的轻量级向量数据库。

关系型能力不是主要目标，SQL 子集主要用于：

- 管理数据库、集合和 schema
- 存储向量关联的元数据
- 在向量检索前做标量过滤
- 提供简洁的客户端查询入口

因此，项目后续不应优先追求完整 SQL 兼容，例如复杂 JOIN、子查询、复杂聚合和完整事务隔离。更重要的方向是：

- 单机持久化
- 向量字段存储
- 过滤 + 向量 TopK 检索
- 向量索引
- 服务端稳定性
- 未来的分片、远程执行和分布式 TopK

## 2. 当前阶段

当前项目已经完成：

- SQL 解析流程
- SQL 执行流程
- 统一 engine 门面，用于执行 SQL 语句
- 简易服务端和客户端
- 多连接支持
- 单数据库实例加锁的并发处理方式
- 基础 `WHERE` 支持
- `VECTOR(n)` 类型进入语法和数据模型设计

当前主要短板是持久化和恢复能力。下一阶段应把项目从“可执行 SQL 的内存数据库”推进到“可重启、可恢复的单机向量数据库”。

## 3. 版本规划

### 3.1 v0.2：单机持久化向量数据库内核

目标：

把现有 SQL 执行闭环接入二进制持久化存储，并支持基础 brute-force 向量检索。

核心任务：

1. 设计二进制存储格式。
2. 持久化 meta。
3. 持久化 collection schema。
4. 持久化普通字段 record。
5. 持久化 `VECTOR(n)` 字段。
6. 启动时恢复 meta 和 record directory。
7. 使用 append-only 方式实现 `INSERT`、`UPDATE`、`DELETE`。
8. 支持 tombstone。
9. 支持损坏尾部 record 的容错处理。
10. 实现 brute-force 向量 TopK。
11. 支持 `WHERE` 过滤后再计算向量距离。
12. 增加服务端重启级端到端测试。

建议文件布局：

```text
data/
  manifest.ldb
  meta.lmeta
  databases/
    demo/
      users.schema
      users.rows
      users.vec
```

建议执行路径：

```text
SeqScan metadata
  -> Filter WHERE
  -> Load vector payload
  -> Compute distance
  -> TopK
  -> Projection
```

验收标准：

1. 启动服务端后可以创建 database 和 collection。
2. 插入包含普通字段和向量字段的数据后，关闭并重启服务端，数据仍可查询。
3. `WHERE` 可以过滤普通字段。
4. 向量距离排序可以返回 TopK 结果。
5. `UPDATE` 和 `DELETE` 在重启后仍然生效。
6. 文件尾部存在半条 record 时，启动恢复不会直接崩溃。
7. 多客户端并发请求在全局锁下保持一致结果。

暂不做：

- B+Tree 索引
- HNSW 索引
- WAL
- MVCC
- 分布式事务
- 复杂 SQL

### 3.2 v0.3：单机向量索引

目标：

在 v0.2 的持久化基础上，引入第一版向量索引能力。

核心任务：

1. 支持 `CREATE VINDEX`。
2. 支持 `DROP VINDEX`。
3. 支持 `SHOW VINDEXES`。
4. 在 meta 中保存向量索引元数据。
5. 实现内存 HNSW 索引。
6. 支持从持久化数据重建索引。
7. Planner 能选择 brute-force 或向量索引。
8. 增加 `EXPLAIN`，显示查询是否使用索引。
9. 支持索引重建。
10. 增加索引正确性测试和召回率测试。

第一版索引策略：

- 优先实现内存 HNSW。
- 暂时允许索引启动时重建。
- 索引落盘可以放到 v0.4 或 v0.3 后半段。
- 标量索引不是优先事项，除非 `WHERE` 过滤成为明显瓶颈。

验收标准：

1. 可以为 `VECTOR(n)` 字段创建向量索引。
2. 向量 TopK 查询可以走 HNSW。
3. 查询结果和 brute-force 结果在可接受误差内一致。
4. `EXPLAIN` 能显示访问路径。
5. 删除或重建索引后查询行为正确。

### 3.3 v0.4：可靠性增强

目标：

让单机数据库具备更明确的崩溃恢复和文件一致性能力。

核心任务：

1. WAL 设计和实现。
2. commit marker。
3. record checksum。
4. meta 变更恢复。
5. data 文件恢复。
6. graceful shutdown。
7. compaction。
8. 文件格式版本管理。
9. 损坏文件检测和错误报告。
10. backup / export 能力。

建议恢复策略：

```text
load manifest
  -> load meta
  -> scan rows
  -> validate vector payload offsets
  -> replay committed WAL records
  -> ignore incomplete tail records
```

验收标准：

1. 进程异常退出后，重启可以恢复到最后一个完整提交点。
2. 未提交或不完整写入不会污染可见数据。
3. meta 和 data 的恢复语义一致。
4. compaction 后数据可正常查询。
5. 文件格式版本不匹配时给出明确错误。

### 3.4 v0.5：分布式雏形

目标：

在单机可靠内核稳定后，扩展为最小可用的分布式查询架构。

核心任务：

1. shard 元数据。
2. coordinator。
3. remote executor。
4. 分片 scan。
5. 分布式 TopK merge。
6. 节点健康检查。
7. 简单副本策略。

第一版分布式边界：

- 优先支持分布式读和分布式向量检索。
- 暂不做强一致分布式事务。
- 暂不做复杂跨分片写事务。
- 写入可以先由 coordinator 路由到单 shard。

验收标准：

1. 一个 coordinator 可以查询多个 shard。
2. 每个 shard 独立保存自己的 collection 数据。
3. TopK 可以在多个 shard 的局部结果上合并。
4. 单个 shard 故障时有明确错误，不导致 coordinator 崩溃。

## 4. 并发模型演进

当前多连接模型使用单数据库实例加锁，这是合理的早期设计。

建议演进路径：

```text
v0.2: global database mutex
v0.3: collection-level lock
v0.4: read-write lock
v0.5: snapshot / MVCC 预研
```

v0.2 阶段建议保持全局锁：

- 每条 SQL 从开始执行到结束持有锁。
- 查询和写入串行执行。
- 优先保证存储一致性。
- 暂时不追求高并发吞吐。

## 5. 文档规划

现有文档：

- `sql_grammar.md`：定义 SQL 方言语法边界。
- `sql_execution_pipeline.md`：定义从 SQL 文本到执行结果的处理链路。
- `optimizer.md`：定义优化器边界、主流数据库设计借鉴、MVP 规则优化和后续成本优化路线。
- `physical_plan.md`：定义物理计划边界、初始物理节点和 logical-to-physical lowering 路线。
- `project_roadmap.md`：定义项目定位、版本规划和阶段目标。

建议新增以下文档。

### 5.1 storage_format.md

目的：

定义二进制持久化格式，作为 v0.2 的核心设计文档。

建议目录：

```text
# litedb 二进制存储格式

## 1. 设计目标
## 2. 文件布局
## 3. 通用编码规则
## 4. Manifest 格式
## 5. Meta 格式
## 6. Schema 格式
## 7. Row 文件格式
## 8. Vector 文件格式
## 9. RowId 设计
## 10. Append-only 写入协议
## 11. Tombstone 和版本记录
## 12. 启动恢复流程
## 13. 文件损坏处理
## 14. 格式版本兼容策略
```

这份文档应先于代码实现完成。存储格式一旦被测试依赖，就不要随意修改。

### 5.2 vector_search.md

目的：

定义向量检索语义、距离函数、TopK 执行模型和未来索引接入方式。

建议目录：

```text
# litedb 向量检索设计

## 1. 设计目标
## 2. VECTOR(n) 类型语义
## 3. 距离函数
## 4. Brute-force TopK
## 5. WHERE 过滤与向量检索顺序
## 6. 查询计划表示
## 7. 结果排序和相等距离处理
## 8. 向量索引接入点
## 9. 测试策略
```

v0.2 只需要覆盖 brute-force。HNSW 的细节可以在 v0.3 新增 `hnsw_index.md`。

### 5.3 server_protocol.md

目的：

定义客户端和服务端之间的协议，避免服务端能力增长后协议变得随意。

建议目录：

```text
# litedb 服务端协议

## 1. 设计目标
## 2. 连接生命周期
## 3. 请求格式
## 4. 响应格式
## 5. 错误码
## 6. 查询结果编码
## 7. 大结果集处理
## 8. 超时和取消
## 9. 兼容性策略
```

### 5.4 recovery.md

目的：

在 v0.4 前明确崩溃恢复语义。

建议目录：

```text
# litedb 恢复机制设计

## 1. 故障模型
## 2. 原子性边界
## 3. WAL 记录格式
## 4. Commit 协议
## 5. Checkpoint
## 6. 启动恢复流程
## 7. Meta 恢复
## 8. Data 恢复
## 9. Vector payload 恢复
## 10. 测试策略
```

### 5.5 distributed_design.md

目的：

等单机持久化稳定后，再定义分布式架构。

建议目录：

```text
# litedb 分布式设计

## 1. 设计目标
## 2. 非目标
## 3. Shard 元数据
## 4. Coordinator
## 5. Remote Executor
## 6. 分布式 Scan
## 7. 分布式 TopK
## 8. 写入路由
## 9. 副本策略
## 10. 故障处理
```

## 6. 文档编写原则

每份设计文档都建议遵循以下结构：

1. 先写目标。
2. 明确非目标。
3. 定义核心概念。
4. 给出数据结构或文件格式。
5. 描述关键流程。
6. 写清错误处理。
7. 写清测试策略。
8. 写清后续扩展点。

设计文档应该避免只写“要做什么”，还要写清楚“暂时不做什么”。对数据库项目来说，非目标和边界非常重要，否则很容易过早进入复杂系统设计。

## 7. 下一步建议

建议立即开始 v0.2，顺序如下：

1. 编写 `storage_format.md`。
2. 定义 `RowId`、record header、value encoding。
3. 拆分或明确 `MetaStore`、`RecordStore`、`VectorStore`。
4. 实现 meta 二进制落盘。
5. 实现 append-only row 文件。
6. 实现 vector payload 文件。
7. 实现启动恢复。
8. 接入 engine 门面。
9. 增加服务端重启端到端测试。
10. 实现 brute-force TopK。

