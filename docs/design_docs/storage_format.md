# litedb 存储格式设计

本文档定义 litedb v0.2 的第一版持久化存储格式。

v0.2 的目标不是实现完整的生产级存储引擎，而是让当前以内存为主的数据库具备可重启能力：

- catalog 元数据在重启后仍然存在；
- collection 中的记录在重启后仍然存在；
- `INSERT`、`UPDATE`、`DELETE` 的效果在重启后仍然生效；
- 恢复时可以忽略 row 文件尾部不完整的半条记录；
- 文件格式版本不匹配时给出明确错误。

运行时执行模型仍然复用当前的 catalog 和 collection storage 接口。启动时从磁盘加载数据到内存，运行期间的变更在应用到内存结构前追加写入磁盘。

## v0.2 非目标

- WAL 和事务提交协议
- MVCC 或快照隔离
- page manager 或 buffer pool
- 普通索引或向量索引
- compaction
- 每条记录的 checksum
- 高并发写入优化
- 超出 storage format version 1 的二进制兼容承诺

## 目录布局

litedb 的数据目录包含：

```text
data/
  manifest.ldb
  catalog.lcat
  collections/
    <collection_id>.rows
```

示例：

```text
data/
  manifest.ldb
  catalog.lcat
  collections/
    1.rows
    2.rows
```

`manifest.ldb` 描述存储格式版本和顶层文件位置。

`catalog.lcat` 是完整的 catalog 快照，保存 database、collection、column、逻辑类型、约束、默认表达式、注释和下一组 ID 计数器。

`collections/<collection_id>.rows` 是某个 collection 的 append-only 行变更日志。启动时，litedb 会 replay 该文件，把记录恢复到内存 collection storage 中。

## 通用编码规则

所有二进制文件统一使用 little-endian 编码。

整数和浮点类型：

```text
u8      unsigned 8-bit integer
u16     unsigned 16-bit integer
u32     unsigned 32-bit integer
u64     unsigned 64-bit integer
i32     signed 32-bit integer
i64     signed 64-bit integer
f32     IEEE-754 32-bit float
f64     IEEE-754 64-bit float
```

布尔值编码为 `u8`：

```text
0       false
1       true
```

字符串编码为：

```text
u32 byte_length
u8[byte_length] utf8_bytes
```

可选值编码为：

```text
u8 present
payload if present == 1
```

数组和向量编码为：

```text
u32 count
element[count]
```

每个文件都以固定文件头开始：

```text
FileHeader
  u32 magic
  u16 format_version
  u16 header_size
```

`header_size` 用于后续版本扩展文件级字段。旧版本 reader 可以根据 `format_version` 明确拒绝不支持的格式。

## 文件 magic

magic 以 little-endian 整数存储。下表中的字节拼写只用于阅读。

```text
manifest.ldb      "LDMF"    0x464d444c
catalog.lcat      "LDCT"    0x5443444c
*.rows            "LDRW"    0x5752444c
```

v0.2 的存储格式版本为 `1`。

## Manifest 文件

`manifest.ldb` 很小，只在顶层存储元数据变化时重写。v0.2 阶段它通常很少变化。

```text
ManifestFile
  FileHeader magic="LDMF", format_version=1
  u32 storage_format_version
  string catalog_path
  string collections_dir
```

v0.2 中固定为：

```text
storage_format_version = 1
catalog_path = "catalog.lcat"
collections_dir = "collections"
```

启动行为：

1. 如果数据目录不存在，则创建数据目录。
2. 如果 `manifest.ldb` 不存在，则初始化一个空的数据目录。
3. 如果 `manifest.ldb` 存在，则校验 magic 和 version。
4. 如果 version 不受支持，则启动失败并返回明确的 storage error。

## Catalog 快照

`catalog.lcat` 保存完整的元数据快照。

v0.2 中 catalog 采用快照语义，而不是 append-only 日志语义。原因是 catalog 元数据较小，变化频率也低于 row data。每次 catalog 变更时，写入一个临时 catalog 文件，然后替换旧快照。

推荐写入流程：

```text
write catalog.lcat.tmp
flush file
rename catalog.lcat.tmp -> catalog.lcat
```

如果某个平台不支持可移植的覆盖式 rename，则由 storage 层封装平台相关的 replace 逻辑。

### Catalog 文件布局

```text
CatalogFile
  FileHeader magic="LDCT", format_version=1
  u64 next_database_id
  u64 next_collection_id
  u64 next_column_id
  u32 database_count
  DatabaseEntry[database_count]
```

Database 按创建或 list 顺序存储。

```text
DatabaseEntry
  u64 database_id
  string database_name
  u32 collection_count
  CollectionEntry[collection_count]
```

Collection 按创建或 list 顺序存储。

```text
CollectionEntry
  u64 collection_id
  string collection_name
  u32 column_count
  ColumnEntry[column_count]
```

Column 按 ordinal 顺序存储。

```text
ColumnEntry
  u64 column_id
  string column_name
  u8 logical_type_id
  OptionalU64 logical_type_parameter
  u8 nullable
  u8 primary_key
  u8 unique
  OptionalDefaultExpression default_expression
  OptionalString comment
```

`logical_type_id` 对应 `common::LogicalTypeId`：

```text
0 Null
1 Boolean
2 Integer
3 BigInt
4 Float
5 Double
6 Varchar
7 Vector
```

`logical_type_parameter` 用于 `VARCHAR(n)` 和 `VECTOR(n)`。这两类类型必须存在 parameter；固定宽度标量类型不应存在 parameter。

### 默认表达式编码

默认表达式使用当前代码中已有的 catalog 表示：

```text
DefaultExpression
  u8 expression_kind
  u8 literal_kind
  string value
  u32 element_count
  DefaultExpression[element_count]
```

`expression_kind` 对应 `CatalogDefaultExpressionKind`：

```text
0 Literal
1 Vector
```

`literal_kind` 对应 `CatalogDefaultLiteralKind`：

```text
0 Null
1 Boolean
2 Integer
3 Float
4 String
```

规则：

- `Literal` 的 `element_count` 必须为 `0`。
- `Vector` 的 `value` 应为空，`element_count` 必须等于向量元素数量。
- 数字默认值以规范化后的原始文本存储，由绑定或求值阶段负责转换为运行时值。

## Row 变更日志

每个 collection 对应一个 row log：

```text
collections/<collection_id>.rows
```

文件开头为：

```text
RowsFile
  FileHeader magic="LDRW", format_version=1
  u64 collection_id
  u64 next_record_id
  RowRecord*
```

`next_record_id` 表示该文件创建或最后一次重写时已知的下一个 record ID。v0.2 采用纯 append-only 方式时，恢复过程也必须在 replay 时计算 `max(record_id) + 1`，并使用两者中较大的值。

每条 row record 使用 frame 包装：

```text
RowRecord
  RowRecordHeader
  u8[payload_size] payload
```

```text
RowRecordHeader
  u32 magic              "RREC" / 0x43455252
  u16 record_version     1
  u16 header_size
  u8 operation
  u8 reserved[7]
  u64 record_id
  u32 payload_size
```

`reserved` 字节写入时必须为 `0`。v1 reader 必须忽略这些字段。

`operation`：

```text
1 Insert
2 Update
3 Delete
```

不同 operation 的 payload：

```text
Insert
  RecordData

Update
  RecordData

Delete
  empty payload
```

Replay 规则：

- `Insert` 执行 `records[record_id] = payload`。
- `Update` 执行 `records[record_id] = payload`。
- `Delete` 从可见记录 map 中移除 `record_id`。
- 记录顺序采用第一次可见 insert 的顺序。
- 对已删除或不存在的 record 执行 update，在 strict mode 下视为存储格式错误。v0.2 可以直接将其作为恢复错误处理，不尝试修复。
- 对不存在的 record 执行 delete，如果文件已经通过基础校验，恢复阶段可以忽略。

## RecordData 编码

Record value 按 collection schema 的列顺序编码。

```text
RecordData
  u32 value_count
  Value[value_count]
```

`value_count` 必须与当前 catalog schema 中 collection 的列数一致。

```text
Value
  u8 value_kind
  payload
```

`value_kind`：

```text
0 Null
1 Boolean
2 Integer
3 BigInt
4 Float
5 Double
6 String
7 Vector
```

Payload：

```text
Null
  empty

Boolean
  u8 value

Integer
  i32 value

BigInt
  i64 value

Float
  f32 value

Double
  f64 value

String
  string value

Vector
  u32 element_count
  f64[element_count] values
```

向量元素编码为 `f64`，因为当前运行时的 `schema::VectorValue` 是 `std::vector<double>`。

恢复时必须根据 catalog schema 校验 value：

- 非 nullable column 不能包含 `Null`；
- value kind 必须匹配 logical type；
- `VARCHAR(n)` 的字符串长度不能超过 `n`；
- `VECTOR(n)` 的元素数量必须等于 `n`。

## 启动恢复流程

启动恢复流程：

```text
load manifest
load catalog snapshot
for each collection in catalog:
  open collections/<collection_id>.rows if it exists
  validate rows file header
  replay complete row records
  ignore incomplete trailing record
create empty row file for collections without one
```

尾部不完整记录处理：

- 如果 reader 在 EOF 前无法读取完整的 `RowRecordHeader`，忽略这部分尾部字节。
- 如果 header 完整，但无法读取 `payload_size` 个 payload 字节，则忽略该 header 和不完整 payload。
- 如果 record magic、version 或 operation 无效，则认为文件损坏，恢复失败。

这条规则让 v0.2 具备有限但有用的中断追加写恢复能力，但不宣称完整 crash consistency。

## 变更写入顺序

DDL：

```text
apply catalog mutation in memory
write catalog snapshot to catalog.lcat.tmp
flush temp file
replace catalog.lcat
create or remove collection row file if needed
```

DML：

```text
validate record against schema
append row record to <collection_id>.rows
flush row file
apply mutation in memory
```

DML 先写磁盘再改内存。这样磁盘写失败会直接返回给调用方，避免出现“内存成功但磁盘失败”的假成功。

v0.2 中每条 SQL 仍然由现有 database-level mutex 保护，因此不需要额外的文件级并发控制。

## Drop 语义

Drop collection：

1. 从 catalog 快照中移除该 collection。
2. 删除或重命名 `collections/<collection_id>.rows`。

v0.2 推荐将被 drop 的文件重命名为：

```text
collections/<collection_id>.rows.dropped
```

这样早期开发阶段可以避免立即执行破坏性删除。后续可以通过 compaction 或 cleanup 命令清理 dropped 文件。

Drop database 时，从 catalog 快照中移除该 database 下的 collections，并将对应 row 文件标记为 dropped。

## 错误处理

存储加载阶段应对以下情况返回明确错误：

- 发现非空 manifest 后缺少必要文件；
- manifest、catalog 或 rows 的版本不受支持；
- magic 无效；
- catalog 结构异常；
- catalog 中存在重复 ID 或重复的 normalized name；
- row 文件中的 collection ID 与 catalog 不匹配；
- row record operation 无效；
- row value 编码无效；
- row value 与 schema 不匹配。

## 未来扩展

v1 格式为后续能力保留扩展空间：

- row record checksum；
- 文件级 checksum；
- catalog event log 替代完整快照；
- row log compaction；
- vector payload 拆分文件；
- 带 commit marker 的 WAL；
- page-based storage；
- index access path。

如果旧版 v1 reader 无法安全拒绝或忽略新增字段，则必须提升 storage format version。
