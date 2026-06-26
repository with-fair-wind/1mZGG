# 跟踪黄金数据夹具

这些 JSON 场景从 `oldsrc/TrackAlgo.cpp` 的关联、失配淘汰和重发现分支提炼而来，用于锁定迁移后的外部行为。夹具只保存可审查的数据，不保存旧类的内存布局或私有状态。

## 格式

- `schema_version`：当前固定为 `1`。
- `mode`：`geo`、`leo`、`sc` 或 `manual`。
- `settings`：场景所需的最小跟踪参数。
- `frames`：严格递增的帧序列；可包含目标像斑、恒星像斑、手动选点和重置动作。
- `expected.target_count`：本帧应输出的活跃目标数。
- `expected.targets`：关键帧的目标 ID、`living`、历史长度、最新帧有效性、TWDW，以及 GDCL 使用的 DN/星等。

数值字段必须有限，帧号必须为正且严格递增。Loader 会在算法执行前拒绝格式错误、缺失字段和越界数值。