# LiteDB Physical Plan Design

## 1. 目标

Physical Plan 的职责是把优化后的逻辑数据流转换成 executor 可以执行的具体算法树。它不负责 SQL 解析、名称绑定、类型检查，也不负责关系语义改写。

当前阶段的目标是建立最小但清晰的物理计划边界：

```text
BoundStatement
  -> LogicalPlanner / LogicalStatementPlan
  -> Optimizer / optimized LogicalStatementPlan
  -> PhysicalPlanner / PhysicalStatementPlan
  -> Executor
```

逻辑计划回答“要算什么”，物理计划回答“怎么执行”。因此逻辑层保持关系语义，物理层表达访问路径和执行算子。

## 2. 模块边界

当前代码模块拆分为：

```text
internal/src/core/logical_plan
internal/src/core/optimizer
internal/src/core/physical_plan
internal/src/core/executor
```

- `logical_plan`：statement 级计划 envelope、关系语义树和 logical planner。
- `optimizer`：改写逻辑计划，当前仍返回 optimized statement plan。
- `physical_plan`：物理计划节点和 logical-to-physical lowering。
- `executor`：执行 `PhysicalStatementPlan`，不直接解释 logical statement 或 logical tree。

## 3. 初始物理节点

第一版物理节点保持一对一 lowering，先不引入 cost search：

```text
LogicalScan       -> PhysicalSeqScan
LogicalScan + index_hint -> PhysicalIndexScan
LogicalFilter     -> PhysicalFilter
LogicalProjection -> PhysicalProjection
LogicalOrderBy    -> PhysicalSort
LogicalLimit      -> PhysicalLimit
```

索引访问路径不再作为独立 logical node 出现。Optimizer 可以把索引候选信息挂到 `LogicalScan` 的 `index_hint` 上，真正的 `PhysicalIndexScan` 只在 physical planner lowering 时产生。

Statement lowering 的顶层结构为：

```text
LogicalStatementPlan
  -> PhysicalStatementPlan
     -> PhysicalQueryPlan(root: PhysicalPlanNode)
     -> PhysicalUpdatePlan(input: PhysicalPlanNode, assignments)
     -> PhysicalDeletePlan(input: PhysicalPlanNode)
     -> PhysicalInsertPlan(values)
     -> Physical*CommandPlan(payload)
```

`PhysicalPlanNode` 只表达关系数据流算子。DDL、SHOW、DESCRIBE 和 INSERT 初期不强行包装成 operator tree，而是作为 statement-level physical plan 保留。

## 4. 第一版不做什么

第一版 physical planner 不做：

- 不接管 executor 执行入口。
- 不引入 cost model。
- 不做多个物理候选计划搜索。
- 不把 command / DDL 强行转换成 physical operator tree。
- 不改变 optimizer 的现有输出契约。

这些限制让物理计划可以先作为独立 IR 落地，并在后续 executor 迁移时提供稳定目标。

## 5. 后续演进

建议实施顺序：

1. 保持当前 physical planner 只做 deterministic lowering。
2. executor 已切到 `PhysicalStatementPlan` 执行入口，不保留 logical statement 执行路径。
3. 增加 `PhysicalVectorIndexScan`、TopK、排序消除和基于统计信息的访问路径选择。
