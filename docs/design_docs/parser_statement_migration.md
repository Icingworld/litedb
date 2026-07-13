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
- Binder binds the concrete SHOW AST nodes listed above.
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

- Add statement plan nodes for scalar and vector index listing.
- Add executor row-set output for scalar and vector index metadata.

## Completed Legacy Cleanup

### Primary-key metadata removal

Parser/Binder status:

- `ParserSchemaHelper::parse_column_definition()` no longer accepts
  `PRIMARY KEY` in `CREATE COLLECTION` column definitions.
- Parser AST no longer exposes primary-key metadata.
- Binder no longer reads primary-key metadata from parser AST.
- Meta、schema 和 persistence 中的历史主键字段已经删除。
- `DESCRIBE` 不再输出主键列；解析器继续明确拒绝 `PRIMARY KEY`。

## Pending Review Before Moving To Binder

- Confirm the result column layout for `SHOW INDEXES` and `SHOW VINDEXES`.
- Confirm whether scalar and vector indexes should share a combined internal
  metadata path or remain separate at the statement level.
