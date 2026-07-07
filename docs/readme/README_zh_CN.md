# litedb

[EN](../../README.md) | 简体中文

`litedb` 是一款使用现代 C++ 编写的轻量级实验性数据库。v0.3.0 版本引入了标量函数框架与内置向量距离函数，可通过 `ORDER BY` 做暴力最近邻查询。更早的版本已建立可重启的数据库闭环：解析 SQL、在目录（catalog）上绑定、规划并执行语句，可通过 `--data-dir` 持久化 catalog 与行数据，并维护内存标量索引，为后续查询加速打下基础。

本项目仍处于早期阶段。当前版本更适合作为数据库内核与学习/实验平台，而非可直接用于生产的存储引擎。

## v0.3.0 已实现的功能

- SQL 词法分析器、解析器、AST、绑定器（binder）、逻辑规划器、求值器（evaluator）、执行器（executor）以及引擎门面（facade）。
- 内存目录、模式（schema）模型与集合（collection）存储。
- 通过 `--data-dir` 显式启用的单机持久化能力。
- 持久化 catalog 快照，以及用于 `INSERT`、`UPDATE`、`DELETE` 的 append-only row log。
- 启动时恢复已持久化的 database、collection、schema、索引定义、标量值与 `VECTOR(n)` 值。
- 内存标量索引：`BTreeIndex`（范围与等值查找）。
- catalog 中的索引元数据，写入 `catalog.lcat` 持久化，并在启动时从已有行数据重建内存索引。
- 索引 DDL：
  - `CREATE INDEX ... ON collection(column) [USING BTREE]`
  - `CREATE INDEX IF NOT EXISTS ...`
  - `DROP INDEX ... ON collection`
  - `DROP INDEX IF EXISTS ... ON collection`
  - `SHOW INDEXES FROM collection`
- 通过 `IndexManager` 在 `INSERT`、`UPDATE`、`DELETE` 时自动维护索引。
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
- 通过在 `ORDER BY` 中使用距离函数进行暴力向量相似度查询（全表扫描，尚无向量索引）。
- 标量类型：`INTEGER`、`BIGINT`、`FLOAT`、`DOUBLE`、`VARCHAR(n)`、`BOOLEAN`。
- `VECTOR(n)` 作为一等模式/值类型，支持固定维度向量字面量，如 `[0.1, 0.2, 0.3]`。
- 独立的 TCP 服务端与交互式客户端 CLI 示例。
- 用于客户端/服务端请求的小型帧协议层。
- 自定义内存子系统，包含页缓存、中央缓存与线程缓存相关测试。
- `verify/` 目录下的 Python 脚本，用于对照参考实现校验向量距离 SQL 结果。

## 当前限制

v0.3.0 有意保持较小的功能范围：

- 持久化需要显式开启。不传 `--data-dir` 时，服务端仍以纯内存模式运行，重启会丢失目录与记录。
- 尚无 WAL、checksum、compaction、checkpoint 或 crash-consistent commit 协议。
- 无事务、MVCC 或隔离性保证。
- 不支持 SQL 连接（join）、子查询、聚合、`GROUP BY` 或完整 SQL 兼容性。
- 标量索引目前为纯内存实现。索引定义会持久化，但索引数据在启动时从 row log 重建，而非写入独立索引文件。
- 查询仍使用顺序扫描加过滤，`IndexScan` 与基于索引的查询规划尚未实现。
- 尚无唯一索引、联合索引或表达式索引。
- 尚无向量索引。相似度查询需全表扫描并在查询时逐行计算距离。
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

当前测试覆盖解析器、目录、模式、标量索引、内存存储、持久化存储、绑定器、逻辑规划器、求值器、执行器、引擎、函数注册表、协议、内存以及客户端/服务端行为。

## 快速开始

启动示例服务端：

```sh
./build/examples/server/litedb_example_server --host 127.0.0.1 --port 5252
```

在 Windows 上，可执行文件通常为：

```powershell
.\build\examples\server\litedb_example_server.exe --host 127.0.0.1 --port 5252
```

默认情况下，示例服务端使用纯内存模式。若希望数据在重启后仍然保留，需要传入 `--data-dir`：

```sh
./build/examples/server/litedb_example_server --host 127.0.0.1 --port 5252 --data-dir ./data
```

在 Windows 上：

```powershell
.\build\examples\server\litedb_example_server.exe --host 127.0.0.1 --port 5252 --data-dir .\data
```

该目录中会生成 `manifest.ldb`、`catalog.lcat`，以及位于 `collections/` 下的 append-only row log。v0.3 的存储格式仍处于实验阶段，不承诺与未来版本保持二进制兼容。

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

SELECT id
FROM users
ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3]) ASC
LIMIT 5;
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
  -> Executor
  -> In-memory or persistent catalog/storage
  -> Execution result
```

仓库目录结构：

```text
internal/src/core/parser       SQL 词法分析器、解析器与 AST
internal/src/core/catalog      目录接口与内存目录
internal/src/core/schema       逻辑类型、值、记录与集合
internal/src/core/function     标量函数注册表与内置函数
internal/src/core/index        内存标量索引与 IndexManager
internal/src/core/storage      集合存储接口与内存存储
internal/src/core/persistence  持久化 catalog 快照与 row log
internal/src/core/binder       名称解析与语义绑定
internal/src/core/logical_plan      逻辑计划构建
internal/src/core/evaluator    表达式求值
internal/src/core/executor     语句与查询执行
internal/src/core/engine       数据库实例与会话门面
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

v0.3.0 之后的近期计划：

- v0.3.x：向量索引（可能从内存 HNSW 起步）、更多内置函数，以及距离查询体验优化。
- v0.2.x 遗留项：`IndexScan` 与简单索引查询规划、持久化能力加固、cleanup/compaction 规划，以及存储格式细节打磨。
- v0.4：可靠性改进，如 WAL、恢复、校验和、压缩与文件格式版本管理。
- v0.5：早期分布式查询架构，包含分片、协调器路由与分布式 TopK 合并。

`docs/design_docs/` 中的设计文档更详细地说明了预期的 SQL 语法、执行流水线与项目路线图。

## 许可证

`litedb` 以 MIT 许可证发布。详见 [LICENSE](../../LICENSE)。
