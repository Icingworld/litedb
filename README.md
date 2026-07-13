# litedb

EN | [简体中文](docs/readme/README_zh_CN.md)

`litedb` is a lightweight experimental database written in modern C++. The
v0.3.0 release adds a scalar function framework and built-in vector distance
functions, enabling brute-force nearest-neighbor queries through `ORDER BY`.
Earlier releases established a restartable database loop: parsing SQL, binding
it against a catalog, planning and executing statements, persisting catalog
and row data under the configured data directory, and maintaining in-memory scalar
indexes for faster future query paths.

This project is still early-stage. The current release is best viewed as a
database kernel and learning/experimentation ground, not as a production-ready
storage engine.

## What works in v0.3.0

- SQL lexer, parser, AST, binder, logical planner, evaluator, executor, and
engine facade.
- In-memory catalog, schema model, and collection storage.
- Single-node persistent catalog and collection storage.
- Persistent catalog snapshots and paged collection store files for `INSERT`,
  `UPDATE`, and `DELETE`.
- Startup recovery for persisted databases, collections, schemas, index
definitions, scalar values, and `VECTOR(n)` values.
- In-memory scalar indexes with `BTreeIndex` (range and equality lookup).
- Index metadata in the catalog, persisted in `catalog.lcat`, and rebuilt from
existing rows on startup.
- Index DDL:
  - `CREATE INDEX ... ON collection(column) [USING BTREE]`
  - `CREATE INDEX IF NOT EXISTS ...`
  - `DROP INDEX ... ON collection`
  - `DROP INDEX IF EXISTS ... ON collection`
  - `SHOW INDEXES FROM collection`
- Automatic index maintenance on `INSERT`, `UPDATE`, and `DELETE` through
`IndexManager`.
- Basic database and collection management:
  - `CREATE DATABASE`, `DROP DATABASE`, `USE`, `SHOW DATABASES`
  - `CREATE COLLECTION ... COMMENT`, column `COMMENT`, `DROP COLLECTION`, `SHOW COLLECTIONS FROM database`, `DESCRIBE`
- Basic data operations:
  - `INSERT`
  - `SELECT` with projection, `WHERE`, `ORDER BY`, `LIMIT`, and `OFFSET`
  - `UPDATE`
  - `DELETE`
- Scalar function framework with a built-in function registry and overload
binding.
- Built-in vector distance functions:
  - `l2_distance(vector, vector)` — Euclidean distance
  - `cosine_distance(vector, vector)` — `1 - cosine similarity`
  - `inner_product(vector, vector)` — dot product
- Function calls in `SELECT` projections, `WHERE`, `ORDER BY`, and other
expression contexts.
- Brute-force vector similarity queries by sorting with distance functions in
`ORDER BY` (full table scan; no vector index yet).
- Scalar types including `INTEGER`, `BIGINT`, `FLOAT`, `DOUBLE`, `VARCHAR(n)`,
and `BOOLEAN`.
- `VECTOR(n)` as a first-class schema/value type with fixed-dimension vector
literals such as `[0.1, 0.2, 0.3]`.
- Standalone TCP server and interactive client CLI examples.
- A small framed protocol layer for client/server requests.
- A custom memory subsystem with page, central, and thread cache tests.
- A Python verification script under `verify/` for checking vector distance SQL
results against reference calculations.

## Current limitations

v0.3.0 intentionally keeps the scope small:

- The example server uses `litedb-data` by default; `--data-dir` selects a
  different persistent data directory.
- No WAL, checksums, compaction, checkpointing, or crash-consistent commit
protocol yet.
- No transactions, MVCC, or isolation guarantees.
- No SQL joins, subqueries, aggregates, `GROUP BY`, or full SQL compatibility.
- Scalar indexes are in-memory only. Index definitions are persisted, but index
data is rebuilt from collection stores on startup rather than stored in separate index
files.
- Queries still use sequential scan plus filter. `IndexScan` and index-based
query planning are not implemented yet.
- No unique indexes, composite indexes, or expression indexes yet.
- No vector indexes yet. Similarity queries scan every row and compute distance
at query time.
- `SELECT ... AS alias` is supported for explicit projection aliases, and
`ORDER BY` can reference those aliases.
- Only the three built-in vector distance functions are available. User-defined
functions and aggregate functions are not implemented yet.

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

The current suite covers parser, catalog, schema, scalar indexes, persistent
storage, binder, logical planner, evaluator, executor, engine,
function registry, protocol, memory, and client/server behavior.

## Quick start

Start the example server:

```sh
./build/examples/server/litedb_example_server --host 127.0.0.1 --port 5252
```

On Windows, the executable is usually:

```powershell
.\build\examples\server\litedb_example_server.exe --host 127.0.0.1 --port 5252
```

By default the example server persists data under `litedb-data`. To use a
different location, pass `--data-dir`:

```sh
./build/examples/server/litedb_example_server --host 127.0.0.1 --port 5252 --data-dir ./data
```

On Windows:

```powershell
.\build\examples\server\litedb_example_server.exe --host 127.0.0.1 --port 5252 --data-dir .\data
```

The data directory will contain `manifest.ldb`, `catalog.lcat`, and append-only
collection store files under `collections/`. The v0.3 storage format is experimental and does
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

Function results in projections currently appear as auto-generated column names
such as `expr3`:

```sql
SELECT id, l2_distance(embedding, [0.1, 0.2, 0.3])
FROM users
ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3]) ASC
LIMIT 5;
```

Other built-in distance functions:

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

Notes:

- Both arguments to a vector distance function must be `VECTOR(n)` values with
the same dimension.
- `ORDER BY` can reference explicit projection aliases, so distance expressions
can be named once in the `SELECT` list and reused for ordering.

Use `.quit` or `.exit` to leave the client.

### Verify vector distance results

After loading the sample data from `docs/design_docs/test_sqls.md`, you can
compare SQL output with Python reference calculations:

```sh
python verify/verify.py --mode all --show
```

## Architecture

The core SQL execution path is:

```text
SQL text
  -> Lexer
  -> Parser / AST
  -> Binder
  -> Logical planner
  -> Executor
  -> Persistent catalog/storage
  -> Execution result
```

Repository layout:

```text
internal/src/core/parser       SQL lexer, parser, and AST
internal/src/core/catalog      Catalog interfaces and in-memory catalog
internal/src/core/schema       Logical types, values, records, collections
internal/src/core/function     Scalar function registry and built-in functions
internal/src/core/index        In-memory scalar indexes and IndexManager
internal/src/core/storage      Persistent collection storage engine and cursor
internal/src/core/persistence  Manifest and persistent lifecycle coordination
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
verify/                        Python scripts to verify vector distance SQL
tests/                         Unit and integration tests
docs/design_docs/              Design notes, test SQL, and roadmap
```

## Roadmap

Near-term work after v0.3.0:

- v0.3.x: vector indexes (likely starting with in-memory HNSW), more built-in
functions, and distance-query polish.
- v0.2.x carry-over: `IndexScan` and simple index-based query planning,
persistence hardening, cleanup/compaction planning, and storage format polish.
- v0.4: reliability improvements such as WAL, recovery, checksums, compaction,
and file format versioning.
- v0.5: early distributed query architecture with shards, coordinator routing,
and distributed TopK merge.

The design documents in `docs/design_docs/` describe the intended SQL grammar,
execution pipeline, and project roadmap in more detail.

## License

`litedb` is released under the MIT License. See [LICENSE](LICENSE).
