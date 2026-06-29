# Parser Statement Migration

This document tracks parser statement changes that are intentionally staged.
Items listed here may leave old code in place until all parser statements have
their target AST shape. After that, the next module can be adapted in one pass.

## Policy

- Do not delete old statement nodes while parser migration is in progress.
- Prefer one concrete AST node per concrete statement form when arguments have
  different meanings.
- Keep parser-facing compatibility until binder, planner, and executor are
  explicitly moved to the new nodes.
- Update this document whenever a staged AST node is added or an old node
  becomes ready for deletion.

## Added, Not Yet Wired

### SHOW statements

New concrete AST nodes have been added:

- `ShowDatabasesStatement`
- `ShowCollectionsStatement`
- `ShowIndexesStatement`
- `ShowVectorIndexesStatement`

Current status:

- Parser returns the concrete SHOW AST nodes listed above.
- Binder still binds the legacy `ShowStatement`.
- Planner still consumes existing `BoundShowDatabasesStatement` and
  `BoundShowCollectionsStatement`.
- Executor still supports only the existing SHOW plan variants.

Completed parser adaptation:

- `SHOW DATABASES` returns `ShowDatabasesStatement`.
- `SHOW COLLECTIONS` returns `ShowCollectionsStatement` with no
  `database_name`.
- `SHOW COLLECTIONS FROM <database>` returns `ShowCollectionsStatement`
  with `database_name`.
- `SHOW INDEXES FROM <collection>` returns `ShowIndexesStatement`.
- `SHOW VINDEXES FROM <collection>` returns
  `ShowVectorIndexesStatement`.

Required downstream adaptation:

- Add bound statement nodes for concrete SHOW forms that do not exist yet.
- Extend binder to resolve `FROM` names in the correct namespace:
  `SHOW COLLECTIONS FROM` resolves a database name, while `SHOW INDEXES FROM`
  and `SHOW VINDEXES FROM` resolve a collection name.
- Add statement plan nodes for scalar and vector index listing.
- Add executor row-set output for scalar and vector index metadata.

## Legacy, Keep Until Migration Completes

### ColumnDefinition `primary_key`

Reason kept:

- `ParserSchemaHelper::parse_column_definition()` no longer accepts
  `PRIMARY KEY` in `CREATE COLLECTION` column definitions.
- `ColumnDefinition::primary_key` still exists because binder, catalog, schema,
  executor, persistence, and existing non-parser tests still reference primary
  key metadata.
- Removing the field now would force a broad downstream migration before the
  parser statement migration is complete.

Deletion criteria:

- Binder no longer reads `ColumnDefinition::primary_key` from parser AST.
- Catalog/schema/persistence primary-key metadata has either been removed or
  moved behind a separate design.
- Tests and examples no longer use `CREATE COLLECTION ... PRIMARY KEY`.
- `DESCRIBE` / metadata output no longer exposes stale primary-key behavior
  unless a replacement feature explicitly keeps it.

### `ShowStatement`

Reason kept:

- Parser no longer constructs `ShowStatement`.
- Binder, planner, executor, visitor/debug-printer compatibility, and some
  includes still keep the legacy node reachable.
- Removing it before downstream adaptation would force a broad mixed-layer
  migration.

Deletion criteria:

- Binder has concrete overloads or branches for all new SHOW AST nodes.
- Debug printer and tests cover the new concrete SHOW nodes.
- No production or test include needs `show_statement.hpp`.

## Pending Review Before Moving To Binder

- Confirm whether `SHOW COLLECTIONS` without `FROM` should continue to use the
  current database.
- Confirm the result column layout for `SHOW INDEXES` and `SHOW VINDEXES`.
- Confirm whether scalar and vector indexes should share a combined internal
  metadata path or remain separate at the statement level.
