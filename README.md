# litedb

EN | [简体中文](docs/readme/README_zh_CN.md)

`litedb` is a lightweight experimental database written in modern C++. Version
0.6.0 provides a restartable single-node database pipeline with persistent
collection storage, persistent B+Tree scalar indexes, persistent HNSW vector
indexes, and rule-based query planning. Distance TopK queries written as normal
SQL can automatically use a matching HNSW index while retaining exact candidate
re-ranking in the regular projection, sort, and limit pipeline.

The current transaction feature branch additionally provides checksum-protected
redo WAL and crash-consistent implicit statement transactions. Each DML or DDL
statement is one `Serializable` transaction; metadata, storage, persistent
B+Tree indexes, and persistent HNSW indexes share the same commit record.

This project is still early-stage. The current release is best viewed as a
database kernel and learning/experimentation ground, not as a production-ready
storage engine.

## What works in v0.6.0

- SQL lexer, parser, AST, binder, logical planner, evaluator, executor, and
engine facade.
- In-memory catalog, schema model, and collection storage.
- Single-node persistent catalog and collection storage.
- Persistent catalog snapshots and paged collection store files for `INSERT`,
  `UPDATE`, and `DELETE`.
- Startup recovery for persisted databases, collections, schemas, index
definitions, scalar values, and `VECTOR(n)` values.
- Persistent paged `BTreeIndex` files with equality lookup and ordered range
scans.
- Index metadata persisted in `meta.lmeta`; B+Tree files are stored under
`indexes/<index_id>.bti` and reopened during startup recovery.
- Index DDL:
  - `CREATE INDEX ... ON collection(column) [USING BTREE]`
  - `CREATE INDEX IF NOT EXISTS ...`
  - `DROP INDEX ... ON collection`
  - `DROP INDEX IF EXISTS ... ON collection`
  - `SHOW INDEXES FROM collection`
- Automatic scalar-index maintenance on `INSERT`, `UPDATE`, and `DELETE`
through `IndexEngine`.
- Implicit statement-level transactions for `INSERT`, `UPDATE`, and `DELETE`,
  with a core-level single-writer guard and atomic multi-row statement commit.
- Versioned redo WAL records with LSNs and checksums, incomplete-tail
  truncation, committed-transaction redo, and idempotent startup recovery
  across collection storage, B+Tree, and HNSW files.
- Basic `DatabaseEngine::observability()` counters for current WAL size,
  WAL generation, checkpoint duration/reclaimed bytes, transaction counts and
  commit duration, and startup redo activity.
- Synchronous manual `DatabaseEngine::checkpoint()` with durable participant
  flushing, generation-based WAL rotation, stale temporary-segment cleanup,
  and crash recovery at each publication boundary.
- Transactional DDL publication for database, collection, B+Tree, and HNSW
  lifecycle changes, including Meta snapshot redo and idempotent file replace
  and delete operations.
- Rule-based scalar access-path selection for supported equality and range
predicates.
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
- Vector-index DDL:
  - `CREATE VINDEX ... ON collection(vector_column) USING HNSW`
  - HNSW options for metric, neighbor count, construction/search breadth, and
    random seed
  - `DROP VINDEX ... ON collection`
  - `SHOW VINDEXES FROM collection`
- `VectorIndexEngine` lifecycle management, including index creation from
existing rows, runtime routing, DML maintenance, recovery, validation, and
recoverable rebuilds.
- Persistent HNSW graph files under `vindexes/`, including truncated-tail and
corruption handling.
- Automatic HNSW selection for limited TopK queries using `l2_distance ASC`,
`cosine_distance ASC`, or `inner_product DESC` with a constant query vector.
- Vector TopK support for `WHERE`, `LIMIT`, and `OFFSET`, with adaptive
candidate expansion and sequential-scan fallback when candidates are
insufficient.
- Exact distance recomputation and sorting of ANN candidates by the regular SQL
pipeline.
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

v0.6.0 is still an experimental single-node release:

- The example server uses `litedb-data` by default; `--data-dir` selects a
  different persistent data directory.
- Transactions are currently implicit and statement-scoped only. There is no
  SQL `BEGIN`, `COMMIT`, or `ROLLBACK`; DML and DDL cannot yet be grouped into a
  caller-controlled multi-statement transaction.
- Execution currently uses a global single writer and fixed statement-level
  `Serializable` isolation. There is no MVCC, concurrent-writer scheduling,
  lock manager, or additional isolation level.
- Checkpointing is currently explicit and synchronous. Automatic size/commit
  thresholds, background checkpointing, WAL archiving, and compaction are not
  implemented.
- No SQL joins, subqueries, aggregates, `GROUP BY`, or full SQL compatibility.
- The optimizer is rule-based. It has no statistics, cardinality estimation, or
cost model for choosing between sequential, scalar-index, and vector-index
access paths.
- SQL `EXPLAIN` and an explicit vector-index rebuild command are not available.
- Scalar indexes are currently single-column, non-unique B+Trees; composite,
unique, and expression indexes are not implemented.
- HNSW results are approximate. Query-level hints and per-query `ef_search`
configuration are not available.
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

The current suite covers parser, catalog, schema, persistent B+Tree and HNSW
indexes, storage and recovery, binder, logical and physical planning,
optimizer, evaluator, executor, database engine, function registry, protocol,
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

By default the example server persists data under `litedb-data`. To use a
different location, pass `--data-dir`:

```sh
./build/examples/server/litedb_example_server --host 127.0.0.1 --port 5252 --data-dir ./data
```

On Windows:

```powershell
.\build\examples\server\litedb_example_server.exe --host 127.0.0.1 --port 5252 --data-dir .\data
```

The data directory contains `manifest.ldb`, `meta.lmeta`, collection store
files under `collections/`, B+Tree files under `indexes/`, and HNSW files under
`vindexes/`. The v0.6 storage and index formats are experimental and do not
promise compatibility with future versions.

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

CREATE VINDEX idx_embedding ON users (embedding) USING HNSW
WITH (metric = L2, max_neighbors = 16, ef_construction = 200, ef_search = 64);

SELECT id
FROM users
ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3]) ASC
LIMIT 5;

SHOW VINDEXES FROM users;
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
  -> Rule-based optimizer
  -> Physical planner
  -> Executor
  -> Storage / IndexEngine / VectorIndexEngine
  -> Execution result
```

Repository layout:

```text
internal/src/core/parser       SQL lexer, parser, and AST
internal/src/core/meta         Catalog metadata and persistence
internal/src/core/schema       Logical types, values, records, collections
internal/src/core/function     Scalar function registry and built-in functions
internal/src/core/index        Persistent B+Tree indexes and IndexEngine
internal/src/core/vindex       Flat/HNSW backends and VectorIndexEngine
internal/src/core/storage      Persistent collection storage engine and cursor
internal/src/core/database     Database runtime, sessions, manifest, and lifecycle coordination
internal/src/core/binder       Name resolution and semantic binding
internal/src/core/logical_plan Logical plan construction
internal/src/core/optimizer    Rule-based logical optimization and access-path selection
internal/src/core/physical_plan Physical plan lowering
internal/src/core/evaluator    Expression evaluation
internal/src/core/executor     Statement and query execution
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

Near-term work after v0.6.0:

- Statistics, cardinality estimation, and cost-based selection among SeqScan,
B+Tree, and HNSW access paths.
- `EXPLAIN` support and explicit index maintenance/rebuild commands.
- Reliability improvements such as WAL, crash-consistent commits, checksums,
compaction, and file-format version management.
- Transactions, MVCC/isolation, and broader SQL support such as joins,
subqueries, aggregates, and `GROUP BY`.
- HNSW performance tuning, larger recall benchmarks, tombstone cleanup, and
query-level search controls.

The design documents in `docs/design_docs/` describe the intended SQL grammar,
execution pipeline, and project roadmap in more detail.

## License

`litedb` is released under the MIT License. See [LICENSE](LICENSE).
