# 资源诊断与可重复压力验证

更新：2026-09-28。本文维护资源计量口径、可重复压力方法与关键性能对照。测试使用合成输入和可控消费端，保留各组件的准入政策；全量测试状态见 [验证矩阵](validation-matrix.md)。

## 指标口径

`Dss::Core::ResourceSnapshot` 提供等待/活动项数量与字节、等待字节峰值、完成/覆盖数量、最近/最长工作耗时及最近停止耗时，时间单位为微秒。峰值和计数在同一组件实例中累计，启停不重置；即时占用随释放归零。

| 组件 | 字节含义 | 读取入口 |
|---|---|---|
| ImageProcessor | 等待帧以及当前输入帧的 RAW、rotated、display、photometry vector capacity | `resourceSnapshot()`；`RuntimeDiagnosticsSnapshot.processing` |
| 图像存储 | 已计费 RAW capacity，区分排队与正在写入；两者之和受字节预算限制 | `imageStorage` |
| 轨迹存储 | records vector capacity × sizeof(TrackDataRecord) | `trackStorage` |
| ImageSender | 单槽现成图像 capacity，加调用者显式提供的 factory 捕获图像字节；发送阶段计入现成图像及分片载荷 | `imageSender` |
| DisplayViewModel | pending RAW/8 位图像，以及当前缓存 RAW；active 在这里表示缓存而非正在运算 | `DisplayViewModel::resourceSnapshot()` |

边界：

- 同一 shared_ptr 在多个组件中可重复计费，不能把组件字节相加当成 RSS。
- 不计对象头、分配器、目标历史、算法临时数组、GPU 纹理、UI 的 QImage 拷贝等。ImageSender 的编码临时数组也不在快照内。
- ImageProcessor 的队列和活动项分开加锁采样，运行时存在短暂的时间差；停止后快照可以做严格归零检查。
- `completedItems` 表示完成工作尝试，不等于成功写入/成功发送；成功、失败和丢弃继续使用原诊断计数器。Display 的计数表示消费显示事件。
- 处理耗时包含同步事件回调；发送耗时包含延迟生成和发送；存储耗时包含 writer 和失败通知。它们不是排队延迟或相机到屏幕延迟。
- 处理、存储、发送的 stop/close 是等待操作；指标只做观测，不新增超时中断机制。UI 缓存不提供工作/停止耗时，相关字段保持 0。
- 网络 factory 捕获成本通过 `retainedSourceBytes` 显式传入，默认 0 表示没有计费，不能据此推断没有捕获内存。

网络提交统一经过 `submitForSend`，直接图像替换旧 factory 时会同时释放旧捕获；worker 异常退出也关闭准入并清空待发槽。

## 压力工具

启用 `DSS_BUILD_APP=ON` 和 `DSS_ENABLE_TESTS=ON` 后生成 `dss_resource_stress`，并注册 `resource_stress_smoke`。使用已有 Qt 运行环境：

```powershell
cmake --build build/clang-cl-debug --target dss_resource_stress
& build/clang-cl-debug/bin/dss_resource_stress.exe 512 512 1000 5 256 > small.json
& build/clang-cl-debug/bin/dss_resource_stress.exe 6144 6144 12 3 256 > large.json
```

参数依次为宽、高、每场景帧数、生命周期循环次数、存储预算 MiB。默认 `512 512 100 3 256`。输入有范围校验；每个循环重新创建并关闭各组件。

四个独立场景：

1. 在处理策略中设置闸门，填满真实 ImageProcessor 的四槽通道，检查非阻塞提交拒绝数，释放后 drain。
2. 阻塞真实 AsyncWriteQueue 的 writer，检查等待和在写载荷共同遵守预算，再排空所有已接受请求。writer 不实际写盘。
3. 暂停主线程处理 Qt 事件，后台连续发布 RAW 显示事件；恢复后只消费最新帧，然后清理缓存。
4. 阻塞 ImageSender 的延迟生成器，检查活动任务和最新待发送槽。factory 返回空图像，因此不发送 UDP 数据；该场景不测 socket 吞吐或丢包。

所有场景检查 RAW 弱引用失效，失败返回非零退出码。闸门有等待上限，超时算失败。JSON 包含逐循环快照、准入/丢弃、停止耗时、P50/P95/max，以及 Windows 工作集/峰值工作集/私有提交字节；其他平台的进程内存字段为 null。

停止分位数是**释放人为闸门后的** drain/close 耗时；少量循环只作为可复现记录，不能用来承诺生产 P95。工作耗时中的人为等待也不能用于比较算法性能。真实慢磁盘、GPU 显示和 UDP 发送性能仍需单独验收。

Sanitizer 的独立构建与使用说明见 [验证指南](sanitizer-validation.md)。

## 受控背压基线（2026-09-21）

- Windows x64，Clang 23.1.1，Qt/OpenCV Debug，CUDA/Sapera 关闭；完整构建成功。
- 两组压力所有循环均通过资源上限、准入/覆盖和 RAW 弱引用释放检查。场景分别运行，不把其内存叠加为应用整体预算。

| 配置 | 每场景帧数 × 循环 | 峰值工作集 MiB | 结束工作集 MiB |
|---|---:|---:|---:|
| 512×512 | 1000 × 5 | 274.87 | 19.21 |
| 6144×6144 | 12 × 3 | 449.13 | 17.14 |

6144×6144 的一帧 RAW 为 72 MiB。阻塞阶段每次均为：

| 组件 | 等待载荷 | 活动载荷 | 行为 |
|---|---:|---:|---|
| 处理 | 4 帧 / 288 MiB | 1 帧 / 72 MiB | 接受 5/12，拒绝 7 |
| 存储 | 2 帧 / 144 MiB | 1 帧 / 72 MiB | 合计 216 MiB ≤ 256 MiB；接受 3/12，拒绝 9 |
| 显示 | 1 帧 / 72 MiB | 阻塞时未消费 | 11 次旧帧替换；恢复后缓存最新 RAW |
| 网络 factory | 1 帧 / 72 MiB | 1 帧 / 72 MiB | 10 次旧待发帧替换 |

释放闸门后的停止耗时（微秒，少量样本；不作为性能达标阈值）：

| 分辨率 | 组件 | P50 | P95 | max |
|---|---|---:|---:|---:|
| 512×512 | network_factory | 10757 | 15414 | 15414 |
| 512×512 | processing | 498 | 772 | 772 |
| 512×512 | storage | 34918 | 36869 | 36869 |
| 6144×6144 | network_factory | 31658 | 37746 | 37746 |
| 6144×6144 | processing | 50172 | 52082 | 52082 |
| 6144×6144 | storage | 28318 | 28463 | 28463 |

载荷释放后进程仍保留 Qt/CRT/分配器等内存，结束工作集不必等于启动值。这些有限运行不能证明没有任意长度的内存泄漏。

本地证据：`build/architecture-review/resource-build-verified.log`、`resource-full-tests-final.log`、`resource-stress-512.json`、`resource-stress-6144.json`、`lsp-resources.jsonl`。JSON 为 Debug 测量，运行期间主机也在执行验证任务，耗时仅作观测基线。

## 2026-09-23：Release 跟踪与实际 I/O 基线

新增 `dss_tracking_io_baseline`，使用生产 GEO、轨迹/RAW 存储后端和 DataExchange/UDP 通道。参数为新建输出目录、帧数、目标数、RAW 边长、RAW 帧数。例如在已配置 Conan 和 Qt 的 Release 构建环境中：

```powershell
cmake --build build/clang-cl-release --target dss_tracking_io_baseline
& build/clang-cl-release/bin/dss_tracking_io_baseline.exe build/baseline-run-1 10000 8 512 64
```

运行时须确保 Qt bin 在 PATH。输出目录必须不存在，父目录必须存在；不会覆盖或自动删除已有文件。保留 report.json、轨迹文本和 RAW 文件，便于复核；重复运行应使用新的目录。Qt 6.11 与 clang-cl 下，由 Qt 清单提供 asInvoker，基线目标禁用链接器重复生成 UAC 节点，避免清单合并后属性错误导致 Windows 拒绝启动。

工具逐帧核对活跃目标数、稳定 ID、累计样本数和历史窗口，逐字节比较收到的 GXTC UDP 报文，停止后核对接受/完成数量和轨迹文件行数。每个发送只保持一个待确认报文，接收超时或文件写入错误使进程失败；队列满时记录拒绝数。

8 目标/10,000 帧的原轮询与新唤醒结果统一列在下节对照表；前三帧处于候选关联期，因此报文与轨迹批次数为 9,997。

口径限制：输入是合成像斑，不含 RAW 图像提取、科学标定或 UI 显示；RAW 仅在指定帧数内写入。文件经过操作系统缓存，未执行 fsync，不代表断电持久化或裸盘带宽。UDP 使用本机回环且逐包等待，完成帧率不能作为跟踪算法最大吞吐。工作集包含工具的定长测量数组，有限运行不能证明任意时长无泄漏。

大帧补充：同一 Release 工具运行 500 帧、8 目标，前 8 帧提交 6144×6144 RAW。**4 帧成功写入，4 帧因存储预算拒绝**；全部已接受请求均完成，没有写入错误。这是生产后端有界准入行为的观测，不是无损采集验收通过。

| 指标 | 6144 场景测量值 |
|---|---:|
| 跟踪耗时 P95 | 148.9 μs |
| UDP 提交至接收 P95 | 16.18 ms |
| UDP 内容验证 / 轨迹行数 | 497 / 3,976 |
| RAW 等待载荷峰值 | 144 MiB |
| 最慢一次 RAW 写入 | 300.57 ms |
| 进程峰值 / 结束工作集 | 443.14 / 11.29 MiB |
| 存储排空及网络关闭合计 | 31.96 ms |

证据：`build/architecture-review/baseline-6144/report.json`。该工具使用复制像素视图的入队重载，峰值包含生产者缓冲、复制及编码临时缓冲；256 MiB 是后端等待和在写 RAW 的计费预算，不是进程工作集上限。所有 RAW 提交集中在前 8 帧，末尾停止前写入已完成，因此这里的停止值也不是磁盘严重积压时的最坏停止时间。

## 2026-09-28：UDP 唤醒改动与固定速率复测

UdpChannel 增加默认 64 条 / 1 MiB 待发送队列上限，发送与关闭通过条件变量唤醒工作线程。接收仍按 5 ms 周期检查；同线程回调发送直接执行，收发各按最多 64 条交替。队列计费不含当前执行的一条报文，其载荷另受单报文 65,507 字节上限约束。饱和拒绝、关闭和异常语义见 [网络模块](module-network.md)。

以下三组均为 8 目标、10,000 帧、前 64 帧提交 512×512 RAW、Qt 6.11.1 / Release。旧基线编译器为 Clang 23.1.1，新两组为 23.1.2，因此不是严格只有一个变量变化的对照实验。新两组测量期间未并行执行构建或其他测试。

| 指标 | 原轮询，无节流 | 发送唤醒，无节流 | 发送唤醒，500 帧/s |
|---|---:|---:|---:|
| UDP 提交至接收 P50 / P95 / P99，ms | 15.491 / 16.687 / 19.603 | 0.039 / 0.107 / 0.404 | 0.101 / 0.532 / 1.803 |
| 跟踪 P95，μs | 109.5 | 25.1 | 137.1 |
| 单帧提交 P95，μs | 16,776.8 | 165.0 | 816.6 |
| 总耗时，含停止，s | 155.767 | 1.511 | 20.033 |
| 完成帧率，帧/s | 64.20 | 6,617.69 | 499.19 |
| UDP 逐字节核验通过 | 9,997 | 9,997 | 9,997 |
| 轨迹批次接受 / 容量拒绝 | 9,997 / 0 | 2,326 / 7,671 | 9,997 / 0 |
| 轨迹文件行数 | 79,976 | 18,608 | 79,976 |
| RAW 接受 / 拒绝 | 64 / 0 | 64 / 0 | 64 / 0 |
| 峰值 / 结束工作集，MiB | 13.42 / 11.70 | 38.86 / 11.93 | 19.79 / 11.74 |
| 存储排空及网络关闭，ms | 31.64 | 626.39 | 25.63 |

数据目录依次为 `build/architecture-review/baseline-8x10000`、`baseline-udp-wake-8x10000`、`baseline-udp-paced-500fps`，均保留 report.json 和实际归档。全部已接受的存储请求完成，写入错误为零；拒绝发生在准入阶段。新无节流场景解除网络等待后将负载集中推入存储，轨迹队列饱和，因此 6,617.69 帧/s 不能作为无损端到端吞吐。没有为了取得零拒绝而放大队列。

工具新增可选第六个业务参数 `FRAME_PERIOD_US`（1–1,000,000 微秒；省略为无节流），report.json 记录 `frame_period_us`。例如在相同 Release 环境中：

```powershell
& build/clang-cl-release/bin/dss_tracking_io_baseline.exe build/baseline-paced-run-1 10000 8 512 64 2000
```

使用累计期限调度，超期后后续帧可能追赶；该参数规定目标平均输入速率，不保证每帧间隔精确为 2 ms。固定速率场景仅约 20 秒，RAW 仅前 64 帧写入；UDP 最大延迟仍有 20.54 ms。因此本轮证据支持“发送等待明显减少、此受控 500 帧/s 场景无存储拒绝”，不代表生产实时阈值、持续 RAW 写盘、跨机网络或最坏延迟达标。跟踪算法未在本轮优化，其分位数变化不作为算法加速结论。

下一步按输入速率和持续时长分别测量轨迹 writer 的服务耗时、批次大小、队列峰值、拒绝数与停止时间，确定长期可承受负载和突发余量。需要无损归档时，应先明确可取消背压及错误反馈的业务契约，再选择实现；不能通过无限队列或静默丢弃解决过载。

这些测量引用的 build 路径是本机保留的原始证据，不随源码分发；复测须按本文命令生成独立输出，后续负载与验收要求见 [实施计划](implementation-plan.md)。
