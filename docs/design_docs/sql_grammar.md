# litedb SQL 方言语法设计文档

## 1. 设计目标

本文档定义 litedb 第一版 SQL 方言的语法边界。第一版目标是打通数据库、集合和基础 CRUD 闭环，保证 Lexer、AST、Parser 和后续执行器可以围绕一组稳定语法实现。

litedb 第一版支持：

- 数据库管理：`USE`、`CREATE DATABASE`、`DROP DATABASE`、`SHOW DATABASES`
- 集合管理：`CREATE COLLECTION`、`DROP COLLECTION`、`SHOW COLLECTIONS`、`DESCRIBE`
- 数据修改：`INSERT`、`UPDATE`、`DELETE`
- 数据查询：`SELECT`
- 表达式：字面量、字段引用、向量字面量、算术表达式、比较表达式、逻辑表达式、`IN`、`BETWEEN`、`LIKE`

litedb 第一版暂不支持：

- 索引和向量索引：`CREATE INDEX`、`CREATE VINDEX`、`DROP INDEX`、`DROP VINDEX`、`SHOW INDEXES`、`SHOW VINDEXES`
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

column_constraint := PRIMARY KEY
                   | UNIQUE
                   | DEFAULT literal
                   | COMMENT string_literal
```

说明：

- 第一版不支持 `NOT NULL` 和 `AUTO_INCREMENT`，虽然 Lexer 已保留相关关键字，语法暂不纳入。
- 每个集合至少需要一个字段。
- `PRIMARY KEY`、`UNIQUE`、`DEFAULT`、`COMMENT` 的语义校验由后续语义层完成。
- 是否允许多个 `PRIMARY KEY`、默认值类型是否匹配字段类型，也由语义层处理。

示例：

```sql
CREATE COLLECTION users (
    id BIGINT PRIMARY KEY,
    name VARCHAR(64),
    age INTEGER DEFAULT 0,
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

列出当前数据库中的集合。

```ebnf
show_collections_statement := SHOW COLLECTIONS
```

示例：

```sql
SHOW COLLECTIONS;
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

## 6. 数据修改语句

### 6.1 INSERT

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

### 6.2 UPDATE

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

### 6.3 DELETE

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

## 7. 数据查询语句

### 7.1 SELECT

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
             | column_reference

order_item := column_reference [ ASC | DESC ]

column_reference := identifier [ "." identifier ]
                  | identifier "." "*"
```

说明：

- 第一版 `SELECT` 不支持 `AS` 别名。
- 第一版 `SELECT` 不支持函数调用作为查询项，即使 AST 预留了 `FunctionCallExpression`。
- `ORDER BY` 暂时只支持字段引用，不支持任意表达式。
- `LIMIT` 和 `OFFSET` 只接受非负整数字面量。

示例：

```sql
SELECT * FROM users;

SELECT id, name, age
FROM users
WHERE age >= 18
ORDER BY age DESC
LIMIT 10 OFFSET 20;
```

## 8. 表达式语法

表达式用于 `WHERE`、`UPDATE SET`、`INSERT VALUES`、默认值和向量字面量。

### 8.1 表达式优先级

从高到低：

| 优先级 | 表达式 |
| --- | --- |
| 1 | 括号、字面量、字段引用、向量字面量 |
| 2 | 一元 `+`、`-`、`NOT` |
| 3 | `*`、`/`、`%` |
| 4 | `+`、`-` |
| 5 | `=`、`!=`、`<>`、`<`、`<=`、`>`、`>=`、`LIKE`、`IN`、`BETWEEN` |
| 6 | `AND` |
| 7 | `OR` |

### 8.2 EBNF

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
      | LIKE additive_expression
      | IN "(" expression { "," expression } ")"
      | BETWEEN additive_expression AND additive_expression
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
  | vector_literal
  | "(" expression ")"
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
```

## 9. AST 设计建议

本节用于约束 Parser 输出，避免语法和 AST 脱节。

### 9.1 需要补充的结构

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
    bool primary_key;
    bool unique;
    std::optional<std::unique_ptr<ExpressionNode>> default_value;
    std::optional<std::string> comment;
};
```

同时建议将泛化的 `CreateStatement` 拆成至少：

```cpp
CreateDatabaseStatement
CreateCollectionStatement
```

其中 `CreateCollectionStatement` 保存：

```cpp
std::string collection;
bool if_not_exists;
std::vector<ColumnDefinition> columns;
```

### 9.2 可以继续泛化的结构

以下语句第一版结构简单，可以继续用泛化节点加对象类型枚举：

```cpp
DropStatement
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

## 10. 未来扩展

后续版本可以按以下顺序扩展：

1. 索引语法
   ```sql
   CREATE INDEX idx_age ON users(age);
   DROP INDEX idx_age ON users;
   SHOW INDEXES FROM users;
   ```

2. 向量索引语法
   ```sql
   CREATE VINDEX idx_embedding ON users(embedding) USING HNSW;
   DROP VINDEX idx_embedding ON users;
   SHOW VINDEXES FROM users;
   ```

3. 聚合查询和分组
   ```sql
   SELECT COUNT(*) FROM users;
   SELECT age, COUNT(*) FROM users GROUP BY age;
   SELECT age, COUNT(*) FROM users GROUP BY age HAVING COUNT(*) > 10;
   ```

4. 查询别名
   ```sql
   SELECT name AS username FROM users;
   ```

5. 更完整的 DDL
   ```sql
   ALTER COLLECTION users ADD COLUMN email VARCHAR(128);
   ALTER COLLECTION users DROP COLUMN email;
   ```
