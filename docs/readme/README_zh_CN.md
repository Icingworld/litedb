# litedb

[EN](../../README.md) | 简体中文

`litedb` 是一款使用现代 C++ 编写的轻量级实验性数据库。v0.1.0 版本聚焦于第一个可用的数据库闭环：解析 SQL、在内存目录（catalog）上绑定、规划并执行语句，并通过小型客户端/服务端协议对外暴露引擎能力。

本项目仍处于早期阶段。当前版本更适合作为数据库内核与学习/实验平台，而非可直接用于生产的存储引擎。

## v0.1.0 已实现的功能

- SQL 词法分析器、解析器、AST、绑定器（binder）、逻辑规划器、求值器（evaluator）、执行器（executor）以及引擎门面（facade）。
- 内存目录、模式（schema）模型与集合（collection）存储。
- 基础数据库与集合管理：
  - `CREATE DATABASE`、`DROP DATABASE`、`USE`、`SHOW DATABASES`
  - `CREATE COLLECTION`、`DROP COLLECTION`、`SHOW COLLECTIONS`、`DESCRIBE`
- 基础数据操作：
  - `INSERT`
  - 支持投影、`WHERE`、`ORDER BY`、`LIMIT`、`OFFSET` 的 `SELECT`
  - `UPDATE`
  - `DELETE`
- 标量类型：`INTEGER`、`BIGINT`、`FLOAT`、`DOUBLE`、`VARCHAR(n)`、`BOOLEAN`。
- `VECTOR(n)` 作为一等模式/值类型，为后续版本的向量检索奠定基础。
- 独立的 TCP 服务端与交互式客户端 CLI 示例。
- 用于客户端/服务端请求的小型帧协议层。
- 自定义内存子系统，包含页缓存、中央缓存与线程缓存相关测试。

## 当前限制

v0.1.0 有意保持较小的功能范围：

- 数据仅保存在内存中。重启服务端会丢失目录与记录。
- 尚无 WAL、恢复、快照或持久化文件格式。
- 无事务、MVCC 或隔离性保证。
- 不支持 SQL 连接（join）、子查询、聚合、`GROUP BY` 或完整 SQL 兼容性。
- 尚无标量索引或向量索引。
- 向量值可作为数据存储与查询，但本版本尚未实现相似度检索。

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

当前测试覆盖解析器、目录、模式、内存存储、绑定器、逻辑规划器、求值器、执行器、引擎、协议、内存以及客户端/服务端行为。

## 快速开始

启动示例服务端：

```sh
./build/examples/server/litedb_example_server --host 127.0.0.1 --port 5252
```

在 Windows 上，可执行文件通常为：

```powershell
.\build\examples\server\litedb_example_server.exe --host 127.0.0.1 --port 5252
```

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
    id BIGINT PRIMARY KEY,
    name VARCHAR(64),
    age INTEGER,
    active BOOLEAN DEFAULT true,
    embedding VECTOR(3)
);

INSERT INTO users (id, name, age, active, embedding)
VALUES (1, 'Ada', 36, true, [0.1, 0.2, 0.3]);

INSERT INTO users (id, name, age, active, embedding)
VALUES (2, 'Linus', 55, true, [0.2, 0.3, 0.4]);

SELECT id, name, age
FROM users
WHERE active = true
ORDER BY age DESC
LIMIT 10;
```

使用 `.quit` 或 `.exit` 退出客户端。

## 架构

核心 SQL 执行路径为：

```text
SQL text
  -> Lexer
  -> Parser / AST
  -> Binder
  -> Logical planner
  -> Executor
  -> In-memory catalog and storage
  -> Execution result
```

仓库目录结构：

```text
internal/src/core/parser       SQL 词法分析器、解析器与 AST
internal/src/core/catalog      目录接口与内存目录
internal/src/core/schema       逻辑类型、值、记录与集合
internal/src/core/storage      集合存储接口与内存存储
internal/src/core/binder       名称解析与语义绑定
internal/src/core/planner      逻辑计划构建
internal/src/core/evaluator    表达式求值
internal/src/core/executor     语句与查询执行
internal/src/core/engine       数据库实例与会话门面
internal/src/protocol          客户端/服务端消息模型
internal/src/net               帧式网络 I/O
internal/src/server            TCP 服务端
internal/src/client            客户端库
internal/src/memory            自定义分配器与缓存实验
examples/                      示例服务端与交互式客户端
tests/                         单元测试与集成测试
docs/design_docs/              设计文档与路线图
```

## 路线图

v0.1.0 之后的近期计划：

- v0.2：单节点持久化存储、目录恢复、仅追加（append-only）记录，以及暴力向量 TopK 检索。
- v0.3：首批向量索引支持，可能从内存 HNSW 实现起步。
- v0.4：可靠性改进，如 WAL、恢复、校验和、压缩与文件格式版本管理。
- v0.5：早期分布式查询架构，包含分片、协调器路由与分布式 TopK 合并。

`docs/design_docs/` 中的设计文档更详细地说明了预期的 SQL 语法、执行流水线与项目路线图。

## 许可证

`litedb` 以 MIT 许可证发布。详见 [LICENSE](../../LICENSE)。
