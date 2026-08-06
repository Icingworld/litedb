# litedb

[EN](../../README.md) | 简体中文

`litedb` 是一款使用现代 C++（C++26）编写的轻量级实验性数据库。v0.8.0 提供了可重启的单机数据库流水线：持久化 collection 存储、持久化 B+Tree 标量索引、持久化 HNSW 向量索引、基于规则的查询规划、带 checksum 的 redo WAL，以及 crash-consistent 的隐式语句级事务。每条 DML 或 DDL 语句作为一个 `Serializable` 事务提交，元数据、存储与索引共享同一条 Commit Record。

本项目仍处于早期阶段 —— 它是一个面向学习与实验的数据库内核，而非可直接用于生产的存储引擎。

## 文档

完整文档（SQL 语法、设计说明、存储格式与路线图）托管在 [GitBook](https://litedb.gitbook.io/litedb-docs)。

## 功能特性

- 完整的 SQL 流水线：词法分析器、解析器、绑定器、逻辑规划器、基于规则的优化器、物理规划器与执行器。
- 单机持久化存储：分页 collection 文件、catalog 快照与幂等的启动恢复。
- 持久化 B+Tree 标量索引：支持等值查询与有序范围扫描，DML 时自动维护。
- 持久化 HNSW 向量索引：以普通 SQL 编写的 TopK 距离查询（`ORDER BY <distance> ... LIMIT k`）会自动使用匹配的 HNSW 索引，并由常规执行流水线对候选做精确距离重排。
- 内置向量距离函数：`l2_distance`、`cosine_distance`、`inner_product`。
- 带版本号、LSN 与 checksum 的 redo WAL，支持 checkpoint 与事务性 DDL 发布。
- 标量类型 `INTEGER`、`BIGINT`、`FLOAT`、`DOUBLE`、`VARCHAR(n)`、`BOOLEAN`，以及一等类型 `VECTOR(n)`。
- 示例 TCP 服务端与交互式客户端 CLI。

v0.8.0 以内部重构为主：parser、binder、planner、optimizer、evaluator 与 executor 围绕 dispatcher/worker 模式进行了重构，并引入了 `common`、`error`、`filesystem`、`io` 等基础设施模块。

## 当前限制

- 仅支持单机、单写者与语句级隐式事务 —— 尚无 `BEGIN` / `COMMIT` / `ROLLBACK` 与 MVCC。
- 不支持 join、子查询、聚合与 `GROUP BY`；优化器基于规则，尚无代价模型。
- 标量索引为单列非唯一 B+Tree；HNSW 返回近似结果。
- 磁盘格式仍处于实验阶段，不承诺向后兼容。

## 环境要求

- CMake 4.3.2 或更高版本
- 支持 C++26 的编译器
- Git 子模块支持（独立 Asio 位于 `third_party/asio`）

```sh
git clone --recurse-submodules https://github.com/Icingworld/litedb.git
cd litedb
```

## 构建

```sh
cmake -S . -B build
cmake --build build
```

运行测试套件：

```sh
ctest --test-dir build --output-on-failure
```

## 快速开始

启动示例服务端（默认持久化到 `./litedb-data`，可用 `--data-dir` 指定其他位置）：

```sh
./build/examples/server/litedb_example_server --host 127.0.0.1 --port 5252
```

在另一个终端中启动交互式客户端：

```sh
./build/examples/client_cli/litedb_example_client_cli --host 127.0.0.1 --port 5252
```

在 Windows 上，运行 `build\examples\...` 下对应的 `.exe` 文件。客户端读取输入直至遇到分号；使用 `.quit` 或 `.exit` 退出。

```sql
CREATE DATABASE demo;
USE demo;

CREATE COLLECTION users (
    id BIGINT NOT NULL,
    name VARCHAR(64) NOT NULL,
    age INTEGER,
    embedding VECTOR(3)
);

INSERT INTO users (id, name, age, embedding)
VALUES (1, 'Ada', 36, [0.1, 0.2, 0.3]);

CREATE INDEX idx_age ON users (age);

CREATE VINDEX idx_embedding ON users (embedding) USING HNSW
WITH (metric = L2, max_neighbors = 16, ef_construction = 200, ef_search = 64);

-- 自动通过 HNSW 索引执行
SELECT id, name
FROM users
ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3]) ASC
LIMIT 5;
```

## 路线图

近期计划包括基于代价的优化、`EXPLAIN`、显式多语句事务与 MVCC、更完整的 SQL 支持（join、聚合、`GROUP BY`）以及 HNSW 调优。完整路线图见 GitBook 文档。

## 许可证

`litedb` 以 MIT 许可证发布。详见 [LICENSE](../../LICENSE)。
