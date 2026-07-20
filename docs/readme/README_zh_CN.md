# litedb

[EN](../../README.md) | 简体中文

`litedb` 是一款使用现代 C++ 编写的轻量级实验性数据库。v0.6.0 已形成可重启的单机数据库闭环，具备持久化 collection 存储、持久化 B+Tree 标量索引、持久化 HNSW 向量索引以及基于规则的查询规划。用户可以继续使用普通距离排序 SQL；存在匹配索引时，TopK 查询会自动使用 HNSW，并由常规投影、排序和限制流水线对候选执行精确距离重排。

本项目仍处于早期阶段。当前版本更适合作为数据库内核与学习/实验平台，而非可直接用于生产的存储引擎。

## v0.6.0 已实现的功能

- SQL 词法分析器、解析器、AST、绑定器（binder）、逻辑规划器、求值器（evaluator）、执行器（executor）以及引擎门面（facade）。
- 内存目录、模式（schema）模型与集合（collection）存储。
- 单机持久化 meta 与集合存储。
- 持久化 meta 快照，以及用于 `INSERT`、`UPDATE`、`DELETE` 的分页集合存储文件。
- 启动时恢复已持久化的 database、collection、schema、索引定义、标量值与 `VECTOR(n)` 值。
- 标量索引：持久化页式 `BTreeIndex` 提供等值查询、范围扫描、插入分裂和首版删除，并作为唯一的标量索引后端正式接入运行时。
- meta 中的索引元数据写入 `meta.lmeta`；BTREE 文件位于 `indexes/<index_id>.bti` 并在启动时直接打开。
- 索引 DDL：
  - `CREATE INDEX ... ON collection(column) [USING BTREE]`
  - `CREATE INDEX IF NOT EXISTS ...`
  - `DROP INDEX ... ON collection`
  - `DROP INDEX IF EXISTS ... ON collection`
  - `SHOW INDEXES FROM collection`
- 通过 `IndexEngine` 在 `INSERT`、`UPDATE`、`DELETE` 时自动维护索引。
- 对支持的等值和范围谓词进行基于规则的标量索引访问路径选择。
- 基础数据库与集合管理：
  - `CREATE DATABASE`、`DROP DATABASE`、`USE`、`SHOW DATABASES`
  - `CREATE COLLECTION ... COMMENT`、列级 `COMMENT`、`DROP COLLECTION`、`SHOW COLLECTIONS FROM database`、`DESCRIBE`
- 基础数据操作：
  - `INSERT`
  - 支持投影、`WHERE`、`ORDER BY`、`LIMIT`、`OFFSET` 的 `SELECT`
  - `UPDATE`
  - `DELETE`
- 标量函数框架：内置函数注册表、签名匹配与重载绑定。
- 内置向量距离函数：
  - `l2_distance(vector, vector)` — 欧氏距离（L2）
  - `cosine_distance(vector, vector)` — `1 - 余弦相似度`
  - `inner_product(vector, vector)` — 内积（点积）
- 函数调用可用于 `SELECT` 投影、`WHERE`、`ORDER BY` 等表达式上下文。
- 向量索引 DDL：
  - `CREATE VINDEX ... ON collection(vector_column) USING HNSW`
  - 支持 metric、邻居数量、构建/搜索宽度和随机种子等 HNSW 参数
  - `DROP VINDEX ... ON collection`
  - `SHOW VINDEXES FROM collection`
- `VectorIndexEngine` 统一负责已有数据建索引、搜索路由、DML 维护、恢复、校验和可恢复重建。
- HNSW 图文件持久化到 `vindexes/`，并支持文件尾截断和损坏检测。
- 对使用常量查询向量的有限 TopK 查询自动选择 HNSW：`l2_distance ASC`、`cosine_distance ASC` 和 `inner_product DESC`。
- 向量 TopK 支持 `WHERE`、`LIMIT` 和 `OFFSET`；过滤候选不足时会自适应扩大候选，并在必要时回退 SeqScan。
- ANN 只负责确定候选集合，最终距离由外层 SQL 流水线重新计算并精确排序。
- 标量类型：`INTEGER`、`BIGINT`、`FLOAT`、`DOUBLE`、`VARCHAR(n)`、`BOOLEAN`。
- `VECTOR(n)` 作为一等模式/值类型，支持固定维度向量字面量，如 `[0.1, 0.2, 0.3]`。
- 独立的 TCP 服务端与交互式客户端 CLI 示例。
- 用于客户端/服务端请求的小型帧协议层。
- 自定义内存子系统，包含页缓存、中央缓存与线程缓存相关测试。
- `verify/` 目录下的 Python 脚本，用于对照参考实现校验向量距离 SQL 结果。

## 当前限制

v0.6.0 仍然是实验性的单机版本：

- 示例服务端默认使用 `litedb-data`，可通过 `--data-dir` 指定其他持久化数据目录。
- DML 已使用 checksum WAL 和 crash-consistent 的隐式语句级提交协议；DDL 尚未纳入同一事务边界。
- 当前固定为全局单写者、语句级 Serializable；尚无显式事务、MVCC、细粒度锁、checkpoint、WAL 回收或 compaction。
- 不支持 SQL 连接（join）、子查询、聚合、`GROUP BY` 或完整 SQL 兼容性。
- 优化器目前基于规则，尚无统计信息、基数估算，也不会通过代价模型比较 SeqScan、B+Tree 和 HNSW 访问路径。
- 尚无 SQL `EXPLAIN` 和显式向量索引重建命令。
- 标量索引目前是单列、非唯一 B+Tree，尚不支持唯一索引、联合索引和表达式索引。
- HNSW 返回近似结果，尚无查询级 hint 或每次查询独立设置 `ef_search` 的能力。
- 支持显式 `SELECT ... AS` 投影别名，`ORDER BY` 可以引用这些别名。
- 目前仅提供上述三个内置向量距离函数，尚不支持用户自定义函数与聚合函数。

## 环境要求

- CMake 4.3.2 或更高版本
- 支持 C++26 的编译器
- Git 子模块支持
- 独立 Asio，通过 `third_party/asio` 提供

克隆仓库（含子模块）：

```sh
git clone --recurse-submodules <repo-url>
cd litedb
```

若克隆时未包含子模块：

```sh
git submodule update --init --recursive
```

## 构建

```sh
cmake -S . -B build -DLITEDB_BUILD_EXAMPLES=ON
cmake --build build
```

运行测试套件：

```sh
ctest --test-dir build --output-on-failure
```

当前测试覆盖解析器、meta、schema、持久化 B+Tree/HNSW、存储与恢复、绑定器、逻辑/物理计划、优化器、求值器、执行器、数据库引擎、函数注册表、协议、内存以及客户端/服务端行为。

## 快速开始

启动示例服务端：

```sh
./build/examples/server/litedb_example_server --host 127.0.0.1 --port 5252
```

在 Windows 上，可执行文件通常为：

```powershell
.\build\examples\server\litedb_example_server.exe --host 127.0.0.1 --port 5252
```

默认情况下，示例服务端将数据持久化到 `litedb-data`。若要使用其他位置，可传入 `--data-dir`：

```sh
./build/examples/server/litedb_example_server --host 127.0.0.1 --port 5252 --data-dir ./data
```

在 Windows 上：

```powershell
.\build\examples\server\litedb_example_server.exe --host 127.0.0.1 --port 5252 --data-dir .\data
```

该目录中会生成 `manifest.ldb`、`meta.lmeta`、位于 `collections/` 下的 collection 存储文件、位于 `indexes/` 下的 B+Tree 文件，以及位于 `vindexes/` 下的 HNSW 文件。v0.6 的存储与索引格式仍处于实验阶段，不承诺与未来版本保持二进制兼容。

在另一个终端中启动客户端 CLI：

```sh
./build/examples/client_cli/litedb_example_client_cli --host 127.0.0.1 --port 5252
```

在 Windows 上：

```powershell
.\build\examples\client_cli\litedb_example_client_cli.exe --host 127.0.0.1 --port 5252
```

然后即可执行 SQL 语句。CLI 会读取输入直至遇到分号。

```sql
CREATE DATABASE demo;
USE demo;

CREATE COLLECTION users (
    id BIGINT NOT NULL,
    name VARCHAR(64) NOT NULL COMMENT 'display name',
    age INTEGER,
    active BOOLEAN DEFAULT true,
    embedding VECTOR(3)
) COMMENT 'user collection';

INSERT INTO users (id, name, age, active, embedding)
VALUES (1, 'Ada', 36, true, [0.1, 0.2, 0.3]);

INSERT INTO users (id, name, age, active, embedding)
VALUES (2, 'Linus', 55, true, [0.2, 0.3, 0.4]);

SELECT id, name, age
FROM users
WHERE active = true
ORDER BY age DESC
LIMIT 10;

CREATE INDEX idx_age ON users (age) USING BTREE;
CREATE INDEX idx_name ON users (name) USING BTREE;

CREATE VINDEX idx_embedding ON users (embedding) USING HNSW
WITH (metric = L2, max_neighbors = 16, ef_construction = 200, ef_search = 64);

SELECT id
FROM users
ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3]) ASC
LIMIT 5;

SHOW VINDEXES FROM users;
```

投影中的函数结果列目前会显示为自动生成的列名，例如 `expr3`：

```sql
SELECT id, l2_distance(embedding, [0.1, 0.2, 0.3])
FROM users
ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3]) ASC
LIMIT 5;
```

其他内置距离函数：

```sql
SELECT id, cosine_distance(embedding, [0.1, 0.2, 0.3])
FROM users
ORDER BY cosine_distance(embedding, [0.1, 0.2, 0.3]) ASC
LIMIT 5;

SELECT id, inner_product(embedding, [0.1, 0.2, 0.3])
FROM users
ORDER BY inner_product(embedding, [0.1, 0.2, 0.3]) DESC
LIMIT 5;
```

说明：

- 向量距离函数的两个参数必须是相同维度的 `VECTOR(n)` 值。
- `ORDER BY` 可以引用显式投影别名，距离表达式可在 `SELECT` 列表中命名后复用。

使用 `.quit` 或 `.exit` 退出客户端。

### 校验向量距离结果

按 `docs/design_docs/test_sqls.md` 导入示例数据后，可运行 Python 脚本对照参考计算结果：

```sh
python verify/verify.py --mode all --show
```

## 架构

核心 SQL 执行路径为：

```text
SQL text
  -> Lexer
  -> Parser / AST
  -> Binder
  -> Logical planner
  -> 基于规则的 Optimizer
  -> Physical planner
  -> Executor
  -> Storage / IndexEngine / VectorIndexEngine
  -> Execution result
```

仓库目录结构：

```text
internal/src/core/parser       SQL 词法分析器、解析器与 AST
internal/src/core/meta         meta 元数据与持久化
internal/src/core/schema       逻辑类型、值、记录与集合
internal/src/core/function     标量函数注册表与内置函数
internal/src/core/index        持久化 B+Tree 与 IndexEngine
internal/src/core/vindex       Flat/HNSW 后端与 VectorIndexEngine
internal/src/core/storage      持久化集合存储引擎与游标
internal/src/core/database     数据库运行时、会话、manifest 与生命周期协调
internal/src/core/binder       名称解析与语义绑定
internal/src/core/logical_plan 逻辑计划构建
internal/src/core/optimizer    基于规则的逻辑优化与访问路径选择
internal/src/core/physical_plan 物理计划 lowering
internal/src/core/evaluator    表达式求值
internal/src/core/executor     语句与查询执行
internal/src/protocol          客户端/服务端消息模型
internal/src/net               帧式网络 I/O
internal/src/server            TCP 服务端
internal/src/client            客户端库
internal/src/memory            自定义分配器与缓存实验
examples/                      示例服务端与交互式客户端
verify/                        用于校验向量距离 SQL 的 Python 脚本
tests/                         单元测试与集成测试
docs/design_docs/              设计文档、测试 SQL 与路线图
```

## 路线图

v0.6.0 之后的近期计划：

- 引入统计信息、基数估算，以及在 SeqScan、B+Tree、HNSW 之间进行选择的代价模型。
- 增加 `EXPLAIN` 和显式索引维护/重建命令。
- 将 DDL 纳入 WAL，增加 checkpoint、WAL 回收、更多数据 checksum、compaction 和文件格式版本管理。
- 支持显式事务、MVCC/更多隔离级别，以及 join、子查询、聚合和 `GROUP BY` 等更完整的 SQL 能力。
- 继续优化 HNSW 性能，增加更大规模的召回率基准、tombstone 清理和查询级搜索参数。

`docs/design_docs/` 中的设计文档更详细地说明了预期的 SQL 语法、执行流水线与项目路线图。

## 许可证

`litedb` 以 MIT 许可证发布。详见 [LICENSE](../../LICENSE)。
