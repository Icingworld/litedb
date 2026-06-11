# litedb

EN | [简体中文](docs/readme/README_zh_CN.md)

`litedb` is a lightweight experimental database written in modern C++. The
v0.2.0 release focuses on making the first usable database loop restartable:
parsing SQL, binding it against a catalog, planning and executing statements,
and optionally persisting catalog and row data through `--data-dir`.

This project is still early-stage. The current release is best viewed as a
database kernel and learning/experimentation ground, not as a production-ready
storage engine.

## What works in v0.2.0

- SQL lexer, parser, AST, binder, logical planner, evaluator, executor, and
engine facade.
- In-memory catalog, schema model, and collection storage.
- Optional single-node persistence enabled by `--data-dir`.
- Persistent catalog snapshots and append-only row logs for `INSERT`, `UPDATE`,
and `DELETE`.
- Startup recovery for persisted databases, collections, schemas, scalar
values, and `VECTOR(n)` values.
- Basic database and collection management:
  - `CREATE DATABASE`, `DROP DATABASE`, `USE`, `SHOW DATABASES`
  - `CREATE COLLECTION`, `DROP COLLECTION`, `SHOW COLLECTIONS`, `DESCRIBE`
- Basic data operations:
  - `INSERT`
  - `SELECT` with projection, `WHERE`, `ORDER BY`, `LIMIT`, and `OFFSET`
  - `UPDATE`
  - `DELETE`
- Scalar types including `INTEGER`, `BIGINT`, `FLOAT`, `DOUBLE`, `VARCHAR(n)`,
and `BOOLEAN`.
- `VECTOR(n)` as a first-class schema/value type, laying groundwork for vector
search in later releases.
- Standalone TCP server and interactive client CLI examples.
- A small framed protocol layer for client/server requests.
- A custom memory subsystem with page, central, and thread cache tests.

## Current limitations

v0.2.0 intentionally keeps the scope small:

- Persistence is opt-in. Without `--data-dir`, the server still runs in
in-memory mode and restart loses catalog and records.
- No WAL, checksums, compaction, checkpointing, or crash-consistent commit
protocol yet.
- No transactions, MVCC, or isolation guarantees.
- No SQL joins, subqueries, aggregates, `GROUP BY`, or full SQL compatibility.
- No scalar indexes or vector indexes yet.
- Vector values can be stored and queried as data, but similarity search is not
implemented in this release.

## Requirements

- CMake 4.3.2 or newer
- A C++26-capable compiler
- Git submodule support
- Standalone Asio, provided through `third_party/asio`

Clone with submodules:

```sh
git clone --recurse-submodules <repo-url>
cd litedb
```

If the repository was cloned without submodules:

```sh
git submodule update --init --recursive
```

## Build

```sh
cmake -S . -B build -DLITEDB_BUILD_EXAMPLES=ON
cmake --build build
```

Run the test suite:

```sh
ctest --test-dir build --output-on-failure
```

The current suite covers parser, catalog, schema, in-memory and persistent
storage, binder, logical planner, evaluator, executor, engine, protocol,
memory, and client/server behavior.

## Quick start

Start the example server:

```sh
./build/examples/server/litedb_example_server --host 127.0.0.1 --port 5252
```

On Windows, the executable is usually:

```powershell
.\build\examples\server\litedb_example_server.exe --host 127.0.0.1 --port 5252
```

By default the example server is in-memory only. To persist data across
restarts, pass `--data-dir`:

```sh
./build/examples/server/litedb_example_server --host 127.0.0.1 --port 5252 --data-dir ./data
```

On Windows:

```powershell
.\build\examples\server\litedb_example_server.exe --host 127.0.0.1 --port 5252 --data-dir .\data
```

The data directory will contain `manifest.ldb`, `catalog.lcat`, and append-only
row logs under `collections/`. The v0.2 storage format is experimental and does
not promise compatibility with future versions.

In another terminal, start the client CLI:

```sh
./build/examples/client_cli/litedb_example_client_cli --host 127.0.0.1 --port 5252
```

On Windows:

```powershell
.\build\examples\client_cli\litedb_example_client_cli.exe --host 127.0.0.1 --port 5252
```

Then run SQL statements. The CLI reads until a semicolon is entered.

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

Use `.quit` or `.exit` to leave the client.

## Architecture

The core SQL execution path is:

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

Repository layout:

```text
internal/src/core/parser       SQL lexer, parser, and AST
internal/src/core/catalog      Catalog interfaces and in-memory catalog
internal/src/core/schema       Logical types, values, records, collections
internal/src/core/storage      Collection storage interface and in-memory storage
internal/src/core/persistence  Persistent catalog snapshots and row logs
internal/src/core/binder       Name resolution and semantic binding
internal/src/core/planner      Logical plan construction
internal/src/core/evaluator    Expression evaluation
internal/src/core/executor     Statement and query execution
internal/src/core/engine       Database instance and session facade
internal/src/protocol          Client/server message model
internal/src/net               Framed network I/O
internal/src/server            TCP server
internal/src/client            Client library
internal/src/memory            Custom allocator and cache experiments
examples/                      Example server and interactive client
tests/                         Unit and integration tests
docs/design_docs/              Design notes and roadmap
```

## Roadmap

Near-term work after v0.2.0:

- v0.2.x: persistence hardening, cleanup/compaction planning, and storage
format polish.
- v0.3: vector search and first vector index support, likely starting with an in-memory HNSW
implementation.
- v0.4: reliability improvements such as WAL, recovery, checksums, compaction,
and file format versioning.
- v0.5: early distributed query architecture with shards, coordinator routing,
and distributed TopK merge.

The design documents in `docs/design_docs/` describe the intended SQL grammar,
execution pipeline, and project roadmap in more detail.

## License

`litedb` is released under the MIT License. See [LICENSE](LICENSE).
