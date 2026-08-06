# litedb

EN | [简体中文](docs/readme/README_zh_CN.md)

`litedb` is a lightweight experimental database written in modern C++ (C++26).
v0.8.0 provides a restartable single-node database pipeline: persistent
collection storage, persistent B+Tree scalar indexes, persistent HNSW vector
indexes, rule-based query planning, checksum-protected redo WAL, and
crash-consistent implicit statement transactions. Each DML or DDL statement
commits as one `Serializable` transaction, with metadata, storage, and indexes
sharing the same commit record.

This project is still early-stage — a database kernel built for learning and
experimentation, not a production-ready storage engine.

## Documentation

Full documentation — SQL grammar, design notes, storage formats, and roadmap —
lives on [GitBook](https://litedb.gitbook.io/litedb-docs).

## Features

- Full SQL pipeline: lexer, parser, binder, logical planner, rule-based
  optimizer, physical planner, and executor.
- Persistent single-node storage: paged collection files, catalog snapshots,
  and idempotent startup recovery.
- Persistent B+Tree scalar indexes with equality lookup and ordered range
  scans, automatically maintained on DML.
- Persistent HNSW vector indexes. TopK distance queries written as plain SQL
  (`ORDER BY <distance> ... LIMIT k`) automatically use a matching HNSW index,
  with exact distance re-ranking in the regular execution pipeline.
- Built-in vector distance functions: `l2_distance`, `cosine_distance`,
  `inner_product`.
- Versioned redo WAL with LSNs and checksums, checkpointing, and transactional
  DDL publication.
- Scalar types `INTEGER`, `BIGINT`, `FLOAT`, `DOUBLE`, `VARCHAR(n)`,
  `BOOLEAN`, plus `VECTOR(n)` as a first-class type.
- Example TCP server and interactive client CLI.

v0.8.0 is primarily an internal refactoring release: the parser, binder,
planners, optimizer, evaluator, and executor were restructured around
dispatcher/worker patterns, and new `common`, `error`, `filesystem`, and `io`
infrastructure modules were introduced.

## Limitations

- Single node, single writer, statement-scoped implicit transactions only — no
  `BEGIN` / `COMMIT` / `ROLLBACK`, no MVCC.
- No joins, subqueries, aggregates, or `GROUP BY`; the optimizer is rule-based
  with no cost model.
- Scalar indexes are single-column, non-unique B+Trees; HNSW results are
  approximate.
- On-disk formats are experimental and do not promise backward compatibility.

## Requirements

- CMake 4.3.2 or newer
- A C++26-capable compiler
- Git submodule support (standalone Asio under `third_party/asio`)

```sh
git clone --recurse-submodules https://github.com/Icingworld/litedb.git
cd litedb
```

## Build

```sh
cmake -S . -B build
cmake --build build
```

Run the test suite:

```sh
ctest --test-dir build --output-on-failure
```

## Quick start

Start the example server (persists under `./litedb-data` by default; use
`--data-dir` to change the location):

```sh
./build/examples/server/litedb_example_server --host 127.0.0.1 --port 5252
```

In another terminal, start the interactive client:

```sh
./build/examples/client_cli/litedb_example_client_cli --host 127.0.0.1 --port 5252
```

On Windows, run the corresponding `.exe` files under `build\examples\...`.
The client reads input until a semicolon; use `.quit` or `.exit` to leave.

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

-- automatically routed through the HNSW index
SELECT id, name
FROM users
ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3]) ASC
LIMIT 5;
```

## Roadmap

Near-term work includes cost-based optimization, `EXPLAIN`, explicit
multi-statement transactions and MVCC, broader SQL support (joins, aggregates,
`GROUP BY`), and HNSW tuning. See the GitBook documentation for the full
roadmap.

## License

`litedb` is released under the MIT License. See [LICENSE](LICENSE).
