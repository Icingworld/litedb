# litedb SQL 方言语法设计文档

## 1. 设计目标

本文档定义 litedb 第一版 SQL 方言的语法边界。第一版目标是打通数据库、集合和基础 CRUD 闭环，保证 Lexer、AST、Parser 和后续执行器可以围绕一组稳定语法实现。

litedb 第一版支持：

- 数据库管理：`USE`、`CREATE DATABASE`、`DROP DATABASE`、`SHOW DATABASES`
- 集合管理：`CREATE COLLECTION`、`DROP COLLECTION`、`SHOW COLLECTIONS FROM`、`DESCRIBE`
- 标量索引管理：`CREATE INDEX`、`DROP INDEX`、`SHOW INDEXES`
- 数据修改：`INSERT`、`UPDATE`、`DELETE`
- 数据查询：`SELECT`
- 表达式：字面量、字段引用、函数调用、向量字面量、算术表达式、比较表达式、逻辑表达式、`IN`、`BETWEEN`、`LIKE`

litedb 下一阶段已确定接入：

- 向量索引管理：`CREATE VINDEX`、`DROP VINDEX`、`SHOW VINDEXES`

litedb 第一版暂不支持：

- 聚合和分组：`COUNT`、`SUM`、`AVG`、`MIN`、`MAX`、`GROUP BY`、`HAVING`
- 查询别名：`AS`
- 多集合查询、连接查询、子查询
- 事务语句

## 2. 词法约定

### 2.1 标识符

标识符用于表示数据库名、集合名、字段名等对象名称。

```ebnf
identifier := letter_or_underscore { letter_or_digit_or_underscore }
```

约定：

- 关键字大小写不敏感，如 `select`、`SELECT`、`Select` 等价。
- 标识符暂不支持反引号或双引号转义。
- 标识符是否大小写敏感由存储层和执行层决定，Parser 仅保留原始文本。

### 2.2 字面量

```ebnf
literal := integer_literal
         | float_literal
         | string_literal
         | boolean_literal
         | null_literal
         | vector_literal

boolean_literal := TRUE | FALSE
null_literal    := NULL
vector_literal  := "[" expression { "," expression } "]"
```

约定：

- 字符串使用单引号或双引号包裹。
- 向量字面量通常由数值表达式组成，例如 `[0.1, 0.2, 0.3]`。
- 第一版语义层应要求向量元素最终可转换为数值。

### 2.3 数据类型

第一版支持以下字段类型：

| 类型 | 说明 |
| --- | --- |
| `INTEGER` | 32 位整数 |
| `BIGINT` | 64 位整数 |
| `FLOAT` | 单精度浮点数 |
| `DOUBLE` | 双精度浮点数 |
| `VARCHAR(n)` | 字符串，`n` 为最大长度 |
| `BOOLEAN` | 布尔值 |
| `VECTOR(n)` | 向量，`n` 为维度 |

```ebnf
data_type := INTEGER
           | BIGINT
           | FLOAT
           | DOUBLE
           | VARCHAR "(" integer_literal ")"
           | BOOLEAN
           | VECTOR "(" integer_literal ")"
```

## 3. 顶层语法

```ebnf
statement := use_statement
           | create_database_statement
           | drop_database_statement
           | show_databases_statement
           | create_collection_statement
           | drop_collection_statement
           | show_collections_statement
           | describe_collection_statement
           | create_index_statement
           | drop_index_statement
           | show_indexes_statement
           | create_vindex_statement
           | drop_vindex_statement
           | show_vindexes_statement
           | insert_statement
           | update_statement
           | delete_statement
           | select_statement

statement_list := statement { ";" statement } [ ";" ]
```

说明：

- 单条语句可以带分号，也可以由调用方决定是否要求分号。
- 交互式 shell 建议要求分号作为语句结束标记。
- Parser 第一版可以先实现单语句解析，再扩展到 `statement_list`。

## 4. 数据库管理语句

### 4.1 USE

切换当前会话使用的数据库。

```ebnf
use_statement := USE identifier
```

示例：

```sql
USE demo;
```

### 4.2 CREATE DATABASE

创建数据库。

```ebnf
create_database_statement := CREATE DATABASE [ IF NOT EXISTS ] identifier
```

示例：

```sql
CREATE DATABASE demo;
CREATE DATABASE IF NOT EXISTS demo;
```

### 4.3 DROP DATABASE

删除数据库。

```ebnf
drop_database_statement := DROP DATABASE [ IF EXISTS ] identifier
```

示例：

```sql
DROP DATABASE demo;
DROP DATABASE IF EXISTS demo;
```

### 4.4 SHOW DATABASES

列出数据库。

```ebnf
show_databases_statement := SHOW DATABASES
```

示例：

```sql
SHOW DATABASES;
```

## 5. 集合管理语句

### 5.1 CREATE COLLECTION

创建集合，并定义字段。

```ebnf
create_collection_statement :=
    CREATE COLLECTION [ IF NOT EXISTS ] identifier "(" column_definition { "," column_definition } ")"

column_definition := identifier data_type { column_constraint }

column_constraint := UNIQUE
                   | DEFAULT literal
                   | NOT NULL
                   | NULL
                   | COMMENT string_literal
```

说明：

- 每个集合至少需要一个字段。
- `UNIQUE`、`DEFAULT`、`NOT NULL`、`NULL`、`COMMENT` 的语义校验由后续语义层完成。
- 默认情况下字段允许为 `NULL`；显式 `NOT NULL` 表示该字段不允许插入或更新为 `NULL`。
- 同一字段不能同时声明 `NULL` 和 `NOT NULL`。
- 默认值类型是否匹配字段类型，也由语义层处理。

示例：

```sql
CREATE COLLECTION users (
    id BIGINT NOT NULL,
    name VARCHAR(64) NOT NULL,
    age INTEGER NULL DEFAULT 0,
    active BOOLEAN DEFAULT true,
    embedding VECTOR(128)
);
```

### 5.2 DROP COLLECTION

删除集合。

```ebnf
drop_collection_statement := DROP COLLECTION [ IF EXISTS ] identifier
```

示例：

```sql
DROP COLLECTION users;
DROP COLLECTION IF EXISTS users;
```

### 5.3 SHOW COLLECTIONS

列出指定数据库中的集合。

```ebnf
show_collections_statement := SHOW COLLECTIONS FROM identifier
```

示例：

```sql
SHOW COLLECTIONS FROM demo;
```

### 5.4 DESCRIBE

查看集合结构。`DESC` 是 `DESCRIBE` 的简写。

```ebnf
describe_collection_statement := ( DESCRIBE | DESC ) [ COLLECTION ] identifier
```

示例：

```sql
DESCRIBE users;
DESC users;
DESCRIBE COLLECTION users;
```

## 6. 索引管理语句

### 6.1 CREATE INDEX

创建标量索引。第一版只支持单列标量索引。

```ebnf
create_index_statement :=
    CREATE INDEX [ IF NOT EXISTS ] identifier
    ON identifier "(" identifier ")"
    [ USING index_method ]

index_method := BTREE
```

说明：

- 省略 `USING` 时默认由语义层选择 `BTREE`。
- 暂不支持 `HASH` 索引，未来可能支持。
- `BTREE` 用于等值查询和范围查询。
- 标量索引不能创建在 `VECTOR(n)` 列上。
- 复合索引暂不支持。

示例：

```sql
CREATE INDEX idx_age ON users(age);
CREATE INDEX IF NOT EXISTS idx_name ON users(name) USING BTREE;
CREATE INDEX idx_age_range ON users(age) USING BTREE;
```

### 6.2 DROP INDEX

删除标量索引。

```ebnf
drop_index_statement :=
    DROP INDEX [ IF EXISTS ] identifier
    ON identifier
```

示例：

```sql
DROP INDEX idx_age ON users;
DROP INDEX IF EXISTS idx_name ON users;
```

### 6.3 SHOW INDEXES

列出指定集合上的标量索引。

```ebnf
show_indexes_statement := SHOW INDEXES FROM identifier
```

示例：

```sql
SHOW INDEXES FROM users;
```

返回结果建议包含索引名、集合名、字段名、索引方法和是否唯一等元数据。第一版索引方法只会返回 `BTREE`。

### 6.4 CREATE VINDEX

创建向量索引。第一版向量索引只支持 HNSW，目标列必须是 `VECTOR(n)`。

```ebnf
create_vindex_statement :=
    CREATE VINDEX [ IF NOT EXISTS ] identifier
    ON identifier "(" identifier ")"
    USING HNSW
    [ WITH "(" vindex_option { "," vindex_option } ")" ]

vindex_option :=
      METRIC "=" vindex_metric
    | MAX_NEIGHBORS "=" integer_literal
    | EF_CONSTRUCTION "=" integer_literal
    | EF_SEARCH "=" integer_literal
    | RANDOM_SEED "=" integer_literal

vindex_metric :=
      L2
    | COSINE
    | INNER_PRODUCT
```

说明：

- `CREATE VINDEX` 是向量索引专用语句，不复用 `CREATE INDEX ... USING HNSW`。
- `USING HNSW` 第一版必填。
- `WITH` 参数可选。
- `metric` 默认值为 `L2`。
- `max_neighbors` 默认值为 `16`。
- `ef_construction` 默认值为 `200`。
- `ef_search` 默认值为 `64`。
- `random_seed` 默认值为 `0`。
- `ef_construction` 必须大于等于 `max_neighbors`。
- `max_neighbors` 和 `ef_search` 必须大于 `0`。
- 向量索引名在同一个 collection 内不能和已有标量索引或向量索引重名。

示例：

```sql
CREATE VINDEX idx_embedding
ON docs(embedding)
USING HNSW;

CREATE VINDEX IF NOT EXISTS idx_embedding_cosine
ON docs(embedding)
USING HNSW
WITH (
    metric = COSINE,
    max_neighbors = 16,
    ef_construction = 200,
    ef_search = 64,
    random_seed = 0
);
```

### 6.5 DROP VINDEX

删除向量索引。

```ebnf
drop_vindex_statement :=
    DROP VINDEX [ IF EXISTS ] identifier
    ON identifier
```

示例：

```sql
DROP VINDEX idx_embedding ON docs;
DROP VINDEX IF EXISTS idx_embedding_cosine ON docs;
```

### 6.6 SHOW VINDEXES

列出指定集合上的向量索引。

```ebnf
show_vindexes_statement := SHOW VINDEXES FROM identifier
```

示例：

```sql
SHOW VINDEXES FROM docs;
```

返回结果建议包含索引名、集合名、字段名、索引方法、距离度量和 HNSW 参数等元数据。

## 7. 数据修改语句

### 7.1 INSERT

插入一条记录。

```ebnf
insert_statement :=
    INSERT INTO identifier [ "(" identifier { "," identifier } ")" ]
    VALUES "(" expression { "," expression } ")"
```

说明：

- 如果提供字段列表，值列表数量必须与字段列表数量一致。
- 如果省略字段列表，值列表按集合字段定义顺序解释。
- 字段是否缺失、类型是否匹配、向量维度是否正确由语义层处理。

示例：

```sql
INSERT INTO users (id, name, age, active)
VALUES (1, 'Tom', 18, true);

INSERT INTO users
VALUES (2, 'Jerry', 20, true, [0.1, 0.2, 0.3]);
```

### 7.2 UPDATE

更新记录。

```ebnf
update_statement :=
    UPDATE identifier
    SET assignment { "," assignment }
    [ WHERE expression ]

assignment := identifier "=" expression
```

说明：

- 未指定 `WHERE` 时更新集合中的所有记录。
- 第一版允许赋值表达式引用字段，如 `age = age + 1`。

示例：

```sql
UPDATE users
SET age = age + 1
WHERE name = 'Tom';

UPDATE users
SET active = false, embedding = [0.2, 0.3, 0.4]
WHERE id = 1;
```

### 7.3 DELETE

删除记录。

```ebnf
delete_statement :=
    DELETE FROM identifier
    [ WHERE expression ]
```

说明：

- 未指定 `WHERE` 时删除集合中的所有记录。
- 第一版 `DELETE` 不支持 `LIMIT`。

示例：

```sql
DELETE FROM users WHERE age < 18;
DELETE FROM users;
```

## 8. 数据查询语句

### 8.1 SELECT

查询记录。

```ebnf
select_statement :=
    SELECT select_item { "," select_item }
    FROM identifier
    [ WHERE expression ]
    [ ORDER BY order_item { "," order_item } ]
    [ LIMIT integer_literal ]
    [ OFFSET integer_literal ]

select_item := "*"
             | identifier "." "*"
             | expression

order_item := expression [ ASC | DESC ]

column_reference := identifier [ "." identifier ]
```

说明：

- 第一版 `SELECT` 不支持 `AS` 别名。
- `SELECT` 列表支持字段引用、通配符和普通表达式。
- `ORDER BY` 支持普通表达式。
- `LIMIT` 和 `OFFSET` 只接受非负整数字面量。

示例：

```sql
SELECT * FROM users;

SELECT id, name, age
FROM users
WHERE age >= 18
ORDER BY age DESC
LIMIT 10 OFFSET 20;

SELECT id, l2_distance(embedding, [0.1, 0.2, 0.3])
FROM users
ORDER BY l2_distance(embedding, [0.1, 0.2, 0.3])
LIMIT 10;
```

## 9. 表达式语法

表达式用于 `WHERE`、`UPDATE SET`、`INSERT VALUES`、默认值和向量字面量。

### 9.1 表达式优先级

从高到低：

| 优先级 | 表达式 |
| --- | --- |
| 1 | 括号、字面量、字段引用、函数调用、向量字面量 |
| 2 | 一元 `+`、`-`、`NOT` |
| 3 | `*`、`/`、`%` |
| 4 | `+`、`-` |
| 5 | `=`、`!=`、`<>`、`<`、`<=`、`>`、`>=`、`LIKE`、`IN`、`BETWEEN` |
| 6 | `AND` |
| 7 | `OR` |

### 9.2 EBNF

```ebnf
expression := or_expression

or_expression :=
    and_expression { OR and_expression }

and_expression :=
    unary_not_expression { AND unary_not_expression }

unary_not_expression :=
    [ NOT ] comparison_expression

comparison_expression :=
    additive_expression
    [
        comparison_operator additive_expression
      | [ NOT ] LIKE additive_expression
      | [ NOT ] IN "(" expression { "," expression } ")"
      | [ NOT ] BETWEEN additive_expression AND additive_expression
    ]

comparison_operator := "=" | "!=" | "<>" | "<" | "<=" | ">" | ">="

additive_expression :=
    multiplicative_expression { ( "+" | "-" ) multiplicative_expression }

multiplicative_expression :=
    unary_expression { ( "*" | "/" | "%" ) unary_expression }

unary_expression :=
    [ "+" | "-" ] primary_expression

primary_expression :=
    literal
  | column_reference
  | function_call
  | vector_literal
  | "(" expression ")"

function_call :=
    identifier "(" [ expression { "," expression } ] ")"
```

说明：

- `NOT IN`、`NOT BETWEEN`、`NOT LIKE` 第一版推荐解析为 `UnaryExpression(Not, InExpression/BetweenExpression/LikeExpression)`。
- Parser 可以用 Pratt Parser 或递归下降加优先级函数实现。
- 语义层负责判断表达式是否可用于特定位置。例如 `WHERE` 中向量字段是否允许参与比较。

示例：

```sql
age >= 18
name LIKE 'Tom%'
age BETWEEN 18 AND 30
category IN ('book', 'tool')
active = true AND age > 18
NOT active OR age < 10
score + bonus >= 100
l2_distance(embedding, [0.1, 0.2, 0.3])
```

## 10. AST 设计建议

本节用于约束 Parser 输出，避免语法和 AST 脱节。

### 10.1 需要补充的结构

因为第一版支持字段定义，AST 需要补充集合定义相关结构：

```cpp
enum class DataTypeKind
{
    Integer,
    BigInt,
    Float,
    Double,
    Varchar,
    Boolean,
    Vector,
};

struct DataType
{
    DataTypeKind kind;
    std::optional<std::size_t> parameter;
};

struct ColumnDefinition
{
    std::string name;
    DataType type;

    bool unique;
    std::optional<bool> nullable;
    std::optional<std::unique_ptr<ExpressionNode>> default_value;
    std::optional<std::string> comment;
};
```

同时建议将泛化的 `CreateStatement` 拆成独立语句节点：

```cpp
CreateDatabaseStatement
CreateCollectionStatement
CreateIndexStatement
ShowIndexesStatement
CreateVectorIndexStatement
ShowVectorIndexesStatement
```

其中 `CreateCollectionStatement` 保存：

```cpp
std::string collection;
bool if_not_exists;
std::vector<ColumnDefinition> columns;
```

`CreateIndexStatement` 保存：

```cpp
std::string index_name;
std::string collection_name;
std::string column_name;
bool if_not_exists;
CreateIndexMethod method; // Default | BTree
```

`CreateVectorIndexStatement` 建议保存：

```cpp
std::string index_name;
std::string collection_name;
std::string column_name;
bool if_not_exists;
CreateVectorIndexMethod method; // Hnsw
VectorIndexOptions options;
```

### 10.2 可以继续泛化的结构

以下语句结构简单，可以继续用泛化节点加对象类型枚举：

```cpp
ShowStatement
DescribeStatement
```

但不建议在 AST 中长期保存 `TokenType` 作为对象类型，建议引入语义层枚举：

```cpp
enum class SchemaObjectType
{
    Database,
    Collection,
};
```

`DROP INDEX` 和 `DROP VINDEX` 当前带有 collection 名称，建议保留独立 AST 节点，避免后续绑定时再从泛化节点里解释参数。

## 11. 未来扩展

后续版本可以按以下顺序扩展：

1. 聚合查询和分组
   ```sql
   SELECT COUNT(*) FROM users;
   SELECT age, COUNT(*) FROM users GROUP BY age;
   SELECT age, COUNT(*) FROM users GROUP BY age HAVING COUNT(*) > 10;
   ```

2. 查询别名
   ```sql
   SELECT name AS username FROM users;
   ```

3. 更完整的 DDL
   ```sql
   ALTER COLLECTION users ADD COLUMN email VARCHAR(128);
   ALTER COLLECTION users DROP COLUMN email;
   ```
