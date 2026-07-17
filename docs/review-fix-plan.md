# DSS_QT 框架审查 · 待核实清单

> **审查日期**:2026-07-15 ｜ **基线分支**:`develop` ｜ **第二轮复核**:同日(Codex)
> **审查范围**:并发核心(事件总线 / ImageProcessor / BoundedChannel / AsyncWriteQueue / SerialWorkerBase / FrameSourceCoordinator / 组合根)+ network/comm + tracking + UI

> ⚠️ **本清单不是「已验证的修复清单」**。所有结论与严重程度均为**初步判断**,实施前必须逐条对照源码与测试复核。第一版中约半数条目经第二轮复核发现描述或方案错误,已在 2026-07-15 修订(见末尾「修订记录」)。仍标 ⚠️ 的为实施前须先确认的事实(协议规范、字段单位、服务内部结构等)。

## 使用说明

- 每条以 `- [ ]` 列出可勾选任务;完成改 `- [x]`,进行中标 `- [~]`。
- 每条含 **定位 / 根因 / 方案 / 测试 / 注意**;⚠️ 为实施前需先核实。
- 进度统计:`Select-String -Pattern '^\s*- \[x\]' docs/review-fix-plan.md | Measure-Object | % LineCount`。
- 修复后建议在 **ASAN + TSAN + UBSan** 下重跑全量 `ctest`(`DSS_ENABLE_TESTS=ON`)。

## 进度总览

| 分组 | 条目数 | 说明 |
| --- | ---: | --- |
| 🔴 高风险(待核实) | 3 | 竞态 UAF / 数据回绕 / 链路失锁 |
| 🟠 中风险(待核实) | 11 | 潜在 UAF / 数据污染 / 性能 / 单位不匹配 |
| 🟡 低优先级 · 防御性 | 4 | 当前不可达或无证据,建议加 assert/校验 |
| 🟢 优化(需 profiling) | 7 | 编译期 / 热路径 / 资源,**非缺陷** |

**图例**:🔴 高 · 🟠 中 · 🟡 低/防御性 · 🟢 优化

---

## 第 1 批 · 高风险(待核实)

### 🔴 A1. 关机竞态 Use-After-Free(概率性,非必然崩溃)

- [x] **实施修复**:`ApplicationContext::shutdown()`,在 `main.cpp` 的 `exec()` 返回后调用 + `~ApplicationContext` 析构兜底(2026-07-16)
- [x] 核实 ViewModel 不缓存服务 `shared_ptr`——成员仅 `ServiceRegistry&` 引用、订阅 lambda 不捕获服务指针,故 `registry.clear()` 可级联停 worker
- [x] 补单元测试 `ShutdownClearsRegistryAndIsIdempotent`、`ShutdownStopsRunningWorkersBeforeDestruction`(`clang-cl-debug` 通过)
- [ ] ASAN/TSAN 构建(非默认 preset)复跑,确认关机路径无 UAF 告警

**定位**:`src/main.cpp:35,59-60`;`include/dss/core/event/detail/event.h:223-236`
**根因**:`main()` 栈对象 `context → mainViewModel → mainWindow`,逆序析构 → `~MainViewModel`(退订订阅)期间 `ImageProcessor` worker 仍在 `emit`。事件总线 COW 快照使 `disconnect` 后旧 handler 仍被 worker 无锁遍历调用 → 访问已析构 ViewModel 的 `this`。是否崩溃取决于 worker 与析构的时序,**概率性,非必然**。
**方案(正确停止顺序——关键:组合根回调强引用服务,`registry.clear()` 单独无效)**:
```cpp
// include/dss/app/application_context.h
void shutdown();

// src/app/application_context.cpp
void ApplicationContext::shutdown() {
    // 1) 先退订:m_connections 的事件回调持有 imageSender(ImageReadyForSendEvent,
    //    communication_services.cpp:58)、trackDataStorage(TrackResultEvent,:141)的 shared_ptr。
    //    clear() 退订并释放这两个引用。ViewModel 仍在栈上,worker 最后一次 emit 安全。
    //    注:frame callback 持有的 imageProcessor/localImageStorage 不在 m_connections,
    //    而在 frame source 内部 m_callback,由下一步 registry.clear() 触发 frame source
    //    析构 stop() join 帧线程时释放,进而级联 imageProcessor 析构 stop() join worker。
    m_connections.clear();
    // 2) 再清 registry:服务引用计数归零 → 析构 → 各服务 stop()+join() worker。
    //    worker 在 join 过程中已无订阅,emit 不命中,安全。
    m_registry.clear();
}

// src/main.cpp
    const int code = QApplication::exec();
    context.shutdown();   // ← 先停 worker,再让栈对象逆序析构
    return code;
```
**⚠️ 注意**(列举已订正):`m_connections` 直接捕获的是 **imageSender、trackDataStorage**(`communication_services.cpp:58,141`);frame callback 持有的 **imageProcessor、localImageStorage** 在 frame source 内部(`:122`),**不在 m_connections**,故 `m_connections.clear()` 释放不了它们——靠 `registry.clear()` 释放 frame source → 析构 `stop()` 帧线程 → callback 释放 → 级联 imageProcessor 析构 `stop()` worker。「先 connections 再 registry」顺序仍必须。若服务间另有析构依赖(如 bridge→dataExchange),map 释放顺序不确定,应改为按序显式 `tryGet` + `stop()`/`close()`。
**测试**:ASAN 下启动 grab → 不 stop 直接经 `shutdown()` 析构,断言无 UAF 告警;重复运行 N 次提高竞态命中率。

### 🔴 A2. `makeJms1970Centiseconds` int32 回绕(算术确定,语义待确认)

- [ ] **⚠️ 前置**:核对旧 GXTC/GDCL 协议规范中该 4 字节字段的时间语义
- [ ] 按确认的语义转换 + 范围校验
- [ ] 扩充测试覆盖现代日期(2026)黄金样本

**定位**:`include/dss/network/protocol/data_exchange_protocol.h:78-97`;调用点 `src/app/track_result_data_exchange_bridge.cpp:70`
**根因**:`return static_cast<int32_t>(centiseconds);`,2026 年值≈1.78e11 ≫ `INT32_MAX`(2.15e9)。C++23 下有符号整数转换超范围为**按模 2^32 的 well-defined 转换**(非实现定义),数据错误结论不变:每帧向旧系统发送回绕后的垃圾时间。测试只覆盖 epoch+1 天。
**⚠️ 注意**:回绕是确定的算术问题,但**修复方式取决于协议语义,不能直接改为「当日内厘秒」**。三种可能须先与旧系统规范核对:
```cpp
// 方案 A:协议期望「当日内厘秒」(0..8_640_000,适配 int32)
    return static_cast<int32_t>(centiseconds % (24LL * 3600LL * 100LL));
// 方案 B:协议期望 int64 全纪元 → 改返回类型 + 确认报文字段宽度(旧定长协议通常不支持)
// 方案 C:截断儒略日等混合表示 → 按规范换算
```
**测试**(扩 `test_data_exchange_protocol.cpp:220`):传入 2026 真实日期,断言不回绕、与旧系统已知正确值一致。

### 🔴 A3. 串口定长切窗无字节级重同步(链路异常后触发)

- [x] **实施修复**:`onDataReceived` 改为流式状态机(累积缓冲 + 扫描帧头 + 校验失败丢字节重同步);抽出 protected `processReceivedBytes` 供测试注入字节流(2026-07-16)
- [~] 协议字节转义规范未确认(无文档)——实现用「扫帧头 + 校验失败丢字节」组合,即使帧体含未转义 HEADER 也能经尾校验收敛;若协议保证无 stuffing 则扫帧头路径更快
- [x] 补重同步测试 4 个:`DecodesConsecutiveValidFrames`、`RecoversFromGarbageByteBetweenFrames`、`RecoversFromTailMismatchAndReportsError`、`BuffersPartialFrameAcrossFeeds`(`clang-cl-debug` 通过)
- [ ] 真实串口流的并发压力测试(需硬件)复跑

**定位**:`src/comm/serial_worker_base.cpp:128-154`
**根因**:每轮 `read(expected)` 后校验头尾,失败仅发错误事件、继续按定长切窗。**正常运行(帧对齐)不会自发失步**;但一旦发生字节插删(UART 丢字节、噪声翻转帧头 `0x7E`、设备复位多发填充),即永久失锁、不可自恢复。属链路异常后的可用性缺陷。
**方案**:
```cpp
// serial_worker_base.h 新增成员(仅 workerLoop 线程访问,无需锁)
    std::vector<uint8_t> m_rxAccumulator;

// serial_worker_base.cpp —— 常量为 FrameCodec::HEADER / FrameCodec::TAIL(frame_codec.h:34-35)
void SerialWorkerBase::onDataReceived(QSerialPort& serialPort) {
    const auto expected = recvFrameSize();
    if (expected == 0U) return;
    while (serialPort.bytesAvailable() > 0) {
        QByteArray chunk = serialPort.readAll();
        m_rxAccumulator.insert(m_rxAccumulator.end(),
            reinterpret_cast<const uint8_t*>(chunk.constData()),
            reinterpret_cast<const uint8_t*>(chunk.constData()) + chunk.size());
    }
    while (m_rxAccumulator.size() >= static_cast<long>(expected)) {
        if (m_rxAccumulator.front() != FrameCodec::HEADER) {                 // 扫描帧头
            auto it = std::find(m_rxAccumulator.begin(), m_rxAccumulator.end(), FrameCodec::HEADER);
            if (it == m_rxAccumulator.end()) { m_rxAccumulator.clear(); break; }
            m_rxAccumulator.erase(m_rxAccumulator.begin(), it);
            if (m_rxAccumulator.size() < expected) break;
        }
        std::span<const uint8_t> frame(m_rxAccumulator.data(), expected);
        const auto v = FrameCodec::validateDetailed(frame, expected);
        if (v.valid) {
            decodeFrame(frame); m_recvCount.fetch_add(1);
            m_rxAccumulator.erase(m_rxAccumulator.begin(), m_rxAccumulator.begin() + expected);
        } else {
            m_bus.emit(Dss::Core::SerialFrameErrorEvent{ /* 现有字段 */ });
            m_rxAccumulator.erase(m_rxAccumulator.begin());   // 丢 1 字节逐步重同步
        }
    }
}
```
**注意**:改变 `onDataReceived` 契约(读走定长 → 累积解析),`recvFrameSize()`/`decodeFrame()` 不变,子类无需改。若协议帧体含未转义 `0x7E`,扫描帧头会误同步,只能靠「丢 1 字节滑窗」(已覆盖)。

---

## 第 2 批 · 中风险(待核实)

### 🟠 B1. 回放加载期间停止指令丢失

- [ ] `ReplayViewModel` 增加 `m_stopRequestedAfterLoad` 意图
- [ ] `MainViewModel::onMasterControl` 加载分支记录意图
- [ ] `finishReplayTask` 检查意图
- [ ] 补「加载中发 grab=false → 不启动」用例

**定位**:`src/ui/main_view_model.cpp:179-184`;`src/ui/replay_view_model.cpp:404`(`finishReplayTask`)
**根因**:`else if (!event.grab && m_replay.isGrabbing())` 仅查 `m_grabbing`,加载阶段为 false,`MasterControlEvent{grab=false}` 被跳过,加载完成后违背指令启动采集。
**方案**:
```cpp
// replay_view_model
void ReplayViewModel::requestStopAfterLoad() { m_stopRequestedAfterLoad = true; }
// finishReplayTask
if (result.startReplayAfterCompletion && !m_stopRequestedAfterLoad) startInitializedReplay();
m_stopRequestedAfterLoad = false;
// main_view_model::onMasterControl
} else if (!event.grab) {
    if (m_replay.isGrabbing()) m_replay.stopGrab();
    else if (m_replay.replayBusy()) m_replay.requestStopAfterLoad();
}
```

### 🟠 B2. `isMotionAngleAwayFromStars` 用 `atan` 丢象限

- [x] 改 `atan2` + 环形角度差;移除随之 unused 的 `softDenominator`/`kSoftDenominatorOffset`(2026-07-17)
- [ ] 补逆行目标专项用例(当前由 test_geo_tracker 19 项回归覆盖)

**定位**:`src/tracking/geo_association.cpp:176-177`
**方案**:
```cpp
const auto motionAngle = std::atan2(motion.y, motion.x);
const auto starAngle   = std::atan2(starSpeed.y, starSpeed.x);
// 角度差用环形距离避免跨 ±π 误判:diff = fmod(fabs(a-b)+pi, 2pi) - pi
```
**测试**:`motion=(-1,-1)` vs `starSpeed=(1,1)`,断言返回 `true`(原为 `false`)。

### 🟠 B3. `ra`/`dec`(度)与 `isInsideRaDecBounds`(<2π 弧度)单位不匹配

- [ ] **⚠️ 前置**:确认 RaDec 跟踪路径设计意图(度 or 弧度)
- [ ] 统一单位:改边界 or 改字段语义
- [ ] 补 ra=123.75(度)的 RaDec 路径用例

**定位**:`include/dss/core/types.h:75-76`;`src/tracking/candidate_utils.cpp:28`;`src/tracking/geo_continuous_tracking.cpp:122-124,137-140`
**根因(证据链)**:
1. `ra`/`dec` 字段是**度**——`types.h:75-76` 注释为度,`test_tracking_prediction_utils.cpp:94` 用 `ra=123.75, dec=-18.5`。
2. `measurementPosition`(RaDec)直接返回 `{blob.ra, blob.dec}` = 度(`candidate_utils.cpp:28`)。
3. `updatePredictionFromHistory` 把它写进 `predictedPosFrame`(`geo_continuous_tracking.cpp:122-124`)。
4. `isInsideRaDecBounds` 检查 `position.x < 2π && position.y < π/2` = **弧度边界**(`geo_continuous_tracking.cpp:137-140`)。
5. → 度值 `123.75` 与 `6.28` 比较 → 判越界 → `living=false`。

任何赤经 > 6.28° 的真实目标在 RaDec 跟踪下会被立即判死。据报告 `test_geo_tracker.cpp` 用弧度值(≈2π)填 `ra` 使边界检查通过,**掩盖**了单位矛盾(此点来自 agent,我未亲见测试值,实施前请核实)。

> **修订说明**:本条第一版误判为「全链路弧度自洽、仅注释错」。第二轮复核指出 `ra`/`dec` 确实是度,该纠正正确;但单位不匹配是**真实问题**(非仅注释),故本条**改写保留而非删除**。最终定性以「前置确认设计意图」为准。

**根因补充(RaDec 链路本就是弧度)**:`geo_association.cpp:225-244` 的关联半径(`arcsecToRad(kGeoRaDecTrackingRadiusArcsec)`)、最小运动(`arcsecToRad(geoRaThresholdArcsec)`/`geoDecThresholdArcsec`)、速度一致性阈值(`arcsecToRad(geoRaSpeedThresholdArcsec)` 等)全部按**弧度**计算,与 `isInsideRaDecBounds` 的 `<2π/<π/2` 弧度边界一致。即**整条 RaDec 追踪链路按弧度设计**,仅 `types.h:75-76` 注释("度")与 `test_tracking_prediction_utils.cpp:94`(`ra=123.75`)按度。

**方案(主推:统一为弧度,而非改天空边界为度)**:若把 `isInsideRaDecBounds` 改为度边界,会牵连关联半径/最小运动/速度阈值(都经 `arcsecToRad`)全部重算,得不偿失。应统一为弧度:
- 修正 `types.h:75-76` 注释为弧度;
- 修正 `test_tracking_prediction_utils.cpp:94` 的 `ra/dec` 为弧度值(并复核 `posTwdw` 透传断言);
- 审计上游所有写入 `blob.ra/dec` 处,确认弧度一致;
- `isInsideRaDecBounds` 保持**弧度单位**,但须**修正边界范围**:当前 `position.y > 0` 拒绝所有南天目标(dec<0)、`position.x > 0` 拒绝春分点(RA=0)。赤纬应为 `[-π/2, π/2]`(允许 0 与负),赤经应为 `[0, 2π)`(允许 0):

```cpp
return position.x >= 0.0F && position.x < static_cast<float>(2.0 * Dss::Core::Pi) &&
       position.y >= static_cast<float>(-Dss::Core::Pi / 2.0) &&
       position.y <= static_cast<float>(Dss::Core::Pi / 2.0);
```

**测试**:补弧度值走完整 RaDec 路径,**必须含南天目标**(如 `dec=-0.32`,≈-18.5°)与 RA≈0 的目标——旧 `>0` 边界会误拒它们,修正后应通过;反例:误用度值应被新断言捕获。

### 🟠 B4. 串口 `write()`/`flush()` 返回值忽略

- [ ] 检查 `write` 返回值,失败 emit 错误事件且不递增计数
- [ ] (可选)配 `waitForBytesWritten`

**定位**:`src/comm/serial_worker_base.cpp:162-167`
**方案(部分写入不能简单 return)**:部分写入时对端已收到截断帧,直接 `return` 丢弃剩余字节会**破坏帧对齐、触发 A3 类失步**。正确处理:
- `write` 返回值 < `buffer.size()` 时,**循环写完剩余字节**(配合 `waitForBytesWritten`),直到整帧发出或遇真实错误(`<0`);
- `write < 0`(真实 IO 错误)时:emit 错误事件、不递增计数,并**关闭重连**或触发协议级重同步(与 A3 的重同步状态机配合);
- `flush() == false` **不能简单视为传输失败**——Qt 中它仅表示仍有字节待写,须配合 `waitForBytesWritten` 判断。
**测试**:mock 部分写入,断言整帧完整发出、对端不失步;mock `write<0`,断言错误事件发布且不递增计数。

### 🟠 B5. `connect` 无 context(7 处潜在 UAF)

- [ ] 7 处补 context 参数(receiver 作 context)
- [ ] 代码审查确认

**定位**:`src/ui/main_window.cpp:341,349`;`src/ui/main_window_control_page.cpp:57,60,65,199`;`src/ui/main_window_display_page.cpp:189`
**方案**:
```cpp
connect(display, &DisplayViewModel::imageStatsUpdated, statsLabel, [statsLabel](...) { ... });
```
**注意**:同文件 `control_page.cpp:71` 有正确写法可对照。

### 🟠 B6. `decodeAngle` 不掩码 29 位

- [ ] **推迟**:经核实 `AngleCodeDenominator = 2^29`(`serial_protocol_codec_detail.h:5-6`),`encodeAngle` 把 360° 编到 `2^29`。原方案「`& 0x1FFFFFFF`」会把合法 360°→0°,反而引入 bug。正确修复需 decodeAngle 范围检查 + 超界发布 `SerialDecodeErrorEvent`(改接口 + `decodePointingFrame`),或 `fmod` 归一化(但向下游隐藏畸形)。且 A3 重同步已过滤大部分失步帧,B6 触发场景(合法头尾 + payload 高位错)罕见。需重新评估修复方向。

**定位**:`include/dss/comm/detail/serial_protocol_codec_detail.h:196-198`
**方案**:
```cpp
[[nodiscard]] inline auto decodeAngle(uint32_t code) -> float {
    constexpr uint32_t kAngleCodeMask = 0x1FFFFFFFu;
    return static_cast<float>((code & kAngleCodeMask) / static_cast<double>(AngleCodeDenominator) * 360.0);
}
```

### 🟠 B7. GEO 死目标无限累积

- [ ] **推迟**:在 `trackTargets()` 末尾 `std::erase_if(!living)` 会让 4 个「终止目标」测试失败——它们经公开接口取 target 断言 `living=false`,erase 后取不到。简单每帧全清破坏「死目标可查」契约;需重新设计(如限制累积上限/保留近期死亡目标),不能直接 erase。
- [ ] 重新设计后再补长时间跟踪内存不涨用例

**定位**:`src/tracking/geo_tracker.cpp:202`(追加)、`210-246`(遍历)
**方案**(循环外 erase,勿在遍历中):
```cpp
std::erase_if(m_targets, [](const Dss::Core::TargetInfo& t) { return !t.living; });
```

### 🟠 B10. `MessageBus` 同步 emit 拖慢主帧路径(性能)

- [x] **已核实**:`ImageSender` 已有单槽 pending 队列(`image_sender.h:98-105`),无需新增
- [x] `ImageReadyForSendEvent` 回调改 `submitForSend`(仅入队);workerLoop 按需调 `imageFactory`,把整图拉伸移出处理线程(2026-07-17)
- [ ] 基准对比处理线程单帧耗时

**定位**:`src/app/communication_services.cpp:58-70`;`src/processing/image_processor.cpp:194`
**根因**:订阅者在 ImageProcessor 工作线程同步执行 `event.imageFactory()`(整图拉伸)+ `sendImage`(UDP 分片)。
**方案(复用现有单槽队列,不引入 AsyncWriteQueue)**:核实确认 `ImageSender` 已有"仅保留最新帧"的单槽 pending(`image_sender.h:98-105` 的 `m_pendingImage`/`m_hasPending`)。无需新增队列,只需把 pending 扩展为**携带 `imageFactory`**,由 worker 线程在发送前按需调用:

```cpp
// 回调仅更新单槽(廉价),不在处理线程调 imageFactory
imageSender->submitForSend(event.frameSeq, event.image, event.imageFactory,
                           event.width, event.height);
// workerLoop 取 pending 后:image 为空则调 imageFactory() 生成 8 位图,再 buildPackets+发送
```

**⚠️ 注意**:单槽"仅保留最新"语义会丢弃处理线程来不及发送的旧帧——对网络图像上报通常可接受,需确认下游不假设帧连续。

### 🟠 B12. `samplePeriodError` 死分支 + 映射错

- [ ] 改为对称包裹;删除永不成立分支
- [ ] **⚠️ 核实** `errorLimit` 调用方语义

**定位**:`src/tracking/math_utils.cpp:202-206`
**方案**:
```cpp
auto error = std::fmod(std::abs(sample), samplePeriod);
if (error > samplePeriod * 0.5f) error -= samplePeriod;   // 映射到 [-P/2, P/2)
```

### 🟠 B13. GEO 初始关联 O(N⁴) 无上限(修复方向已修订)

- [ ] **⚠️ 不要**只把上限设为 2000——O(2000⁴)≈1.6e13 仍不可接受
- [ ] 检查**四帧**(非仅首帧)的 blob 数
- [ ] 优先做候选空间索引/网格筛选剪枝,或设远低于 2000 的总预算
- [ ] 补大 blob 数不卡死用例

**定位**:`src/tracking/geo_association.cpp` `associateFourFrameTargets`;调用方 `src/tracking/geo_tracker.cpp` `assoc4`
**修订说明**:第一版建议「首帧上限 2000」不足——既只查一帧,且 2000 在 O(N⁴) 下仍爆炸。根治方向是空间索引(如按像面分桶)先降低候选组合,再配较低总预算兜底。
**方案(框架)**:
```cpp
// associateFourFrameTargets 入口:四帧任一过大即拒绝/剪枝
for (const auto* f : frames) {
    if (f->targetBlobs.size() > kMaxGeoInitialBlobPerFrame) { /* 空间分桶剪枝 or 返回空 + 诊断 */ }
}
```

### 🟠 B15. `isSameEquatorialPoint` 双重缺陷(位置已订正 + 补遗漏)

- [ ] 修复 1:赤经阈值施 cos(dec),赤纬阈值不缩放
- [ ] 修复 2:仅有 ra/dec 时比较 ra/dec,而非 alpha/sigma
- [ ] 补高赤纬用例 + 仅 ra/dec 用例

**定位**:`src/tracking/geo_continuous_tracking.cpp:12-26`(第一版误写为 `geo_validation.cpp`,已订正)
**缺陷 1(数学)**:line 22-25 对 `alpha` 和 `sigma` 都用同一 `cos(first.sigma)`-放大后的 threshold。赤纬沿经线不应被 cos(dec) 缩放。
**缺陷 2(遗漏,Codex 补充)**:`hasEquatorialCoordinates`(line 13)接受「仅有 ra/dec」,但 `isSameEquatorialPoint`(line 24-25)**只比 alpha/sigma**。仅有 ra/dec 的 blob 其 alpha=sigma=0,会被误判为「相同点」。
**方案(缺陷 2 不能用零值推断缺失)**:赤经/赤纬为 0 是合法坐标,不能以 `alpha==0 && sigma==0` 推断"仅有 ra/dec"。**较小且明确的修复**(不扩大到整个 RaDec 路径,与 B3 保持一致):把 `isSameEquatorialPoint` 的存在性判断与比较都**限定为 alpha/sigma**——`hasEquatorialCoordinates` 在此语境只判 `alpha!=0 || sigma!=0`,比较只比 alpha/sigma。`ra/dec` 继续由 RaDec 跟踪链路(关联半径/速度等)单独处理,不进入此函数。若将来确需在同一函数支持两种坐标来源,再引入显式有效性标记。

缺陷 1(阈值)修复不变:

```cpp
const auto raThreshold  = kGeoSameEquatorialThresholdDeg / (std::cos(first.sigma) + kTinyCos) * DegToRad;
const auto decThreshold = kGeoSameEquatorialThresholdDeg * DegToRad;   // 赤纬不缩放
```

---

## 第 3 批 · 低优先级 · 防御性(当前不可达或无证据)

> 以下条目经第二轮复核降级:或当前契约下不可触发,或无证据表明输入会进入异常状态。建议加 assert/校验强化契约,但**非必须修复的缺陷**。

### 🟡 B8. `ReplayViewModel` 析构 join 阻塞 GUI

- [ ] (可选)`stopGrab` 或专门 cancel 方法 `m_replayTaskWorker.request_stop()`,加载 IO 循环检查 `stop_requested()`

**定位**:`src/ui/replay_view_model.cpp:53-58`
**现状**:`request_stop()+join()` 仅在析构(53-58)和 `startReplayTask`(373-375)中;`stopGrab`(193)**不**请求 task worker 停止。故 closeEvent 调 `stopGrab` 不能缩短析构 join。`std::jthread::join` 无超时,根治需让加载任务可取消。属关机卡顿,优先级低。

### 🟡 B9. `median3` 对 NaN 无防护

- [ ] (可选)源头对质心结果做 `isfinite` 校验;**勿盲目返回 0**

**定位**:`src/tracking/prediction_utils.cpp:26-30`;`src/tracking/geo_association.cpp:158-162`
**修订说明**:第一版建议「返回 0」有害——会伪造「静止」速度污染预测,比传播 NaN 更隐蔽。且无证据表明质心输入会 NaN。**不建议直接返回 0**;若加防护,应 `isfinite` 检查后跳过该样本(如退化为 2 点中位数),而非填 0。

### 🟡 B11. `targetFrameMotionAt` 无下界检查(当前不可触发)

- [ ] (可选)加 `assert(index > 0 && index < frameInfos.size())` 强化契约

**定位**:`src/tracking/prediction_utils.cpp:116-126`
**现状**:`index=0` 时 `index-1U` 下溢为 `SIZE_MAX` → 越界 UB。但调用方(`updatePredictionFromHistory` 的内联 `motionAt` 有 `size>=4` 守卫;`targetFrameMotionAt` 的调用方据复核亦先检查 `size`)均保证 `index>=1`,**当前不可触发**。降为契约强化。

### 🟡 B14. `encodeFrame` 子类对返回值 `(void)` 强转(正常契约不可达)

- [ ] (可选)失败时 emit 错误而非强转丢弃

**定位**:`src/comm/exposure_channel.cpp:40`、`master_control_channel.cpp:30`、`servo_channel.cpp:34`
**现状**:基类按 `sendFrameSize()` 定长分配缓冲(`serial_worker_base.cpp:157`),layout 与尺寸同源,正常契约下 `encode*` 不会失败。属防御性死代码,优先级低。

---

## 优化项(需 profiling/兼容性验证,非缺陷)

> 以下为合理优化方向,但**需先 profiling 确认收益**;GL 相关需兼容性验证。不应表述为「必须修复」。

- [ ] **C1** 存储后端 header-only → 拆 `.cpp`;`AsyncWriteQueue` 显式实例化(需测编译时间收益)
- [ ] **C2** `ImageProcessor` 每帧堆分配 → worker 线程局部复用缓冲(需基准)
- [ ] **C3** `UdpChannel::onReadyRead` 每报文 `vector` 拷贝 → 回调传 `std::span`(需确认回调不跨线程存留)
- [ ] **C4** `GpuImageDisplay` stride≠width 逐行 memcpy → `glPixelStorei(GL_UNPACK_ROW_LENGTH)`(需确认上下文支持)
- [ ] **C5** 废弃 `GL_LUMINANCE` + GLSL1.20 → 迁移现代 GL 或显式 Compatibility Profile(需跨驱动验证)
- [ ] **C6** `LogViewModel::erase(begin())` O(n) → `std::deque`/环形(需高频日志场景验证收益)
- [ ] **C7** `exposureTime` ms↔秒往返精度 → 统一单一单位

---

## 本轮静态检查未发现(非已证实,勿当定论)

> 以下为代码**静态阅读**结论,未经压力测试/动态验证,表述为「未发现」而非「已证实」。改动相关代码后须重新评估。

- 事件总线 COW 快照 + 无锁回调:**嵌套 emit 安全**(`findChannel`/`snapshot` 返回前已释放锁,无 shared_mutex 同线程重入 UB)——此条为机制确定性结论。
- `FrameSourceCoordinator::LifecycleOperation` RAII + cv 串行化生命周期操作——静态未发现并发漏洞,未做并发生命周期压测。
- 各 `std::jthread` 析构 `stop()+join()` 兜底;`BoundedChannel` 停止排空语义——静态未发现错误,未做高负载压测。
- 编解码器先 `validateReceiveFrame` 校验定长再按偏移访问——静态未发现越界,畸形/超长输入未充分 fuzz。
- `UdpChannel` 停止顺序/重入、`ImageSender`/`ErrorDiagnostics` 关闭顺序——静态未发现死锁,未做并发停止压测。

> **已删除的错误推理**(第一版):「回调以 shared_ptr 强引用持有服务,registry 释放后回调内对象存活到 worker join」。该推理自相矛盾——回调持有 `shared_ptr` 会**阻止**服务析构,也就不会触发析构中的 `join`。正因为如此,A1 的停止顺序必须「先退订释放回调引用,再清 registry」。

---

## 修订记录

**2026-07-15(第二轮复核,外部 Codex)**:第一版约半数条目存在描述或方案错误,本次修订:
- **A1**:方案重写。原 `m_registry.clear()` 单独无效(回调 lambda 强引用服务);改为「先 `m_connections.clear()` 退订 → 再 `registry.clear()`」。严重性「确定性崩溃」→「概率性 UAF」。
- **A3**:补「链路异常后触发」前提;常量名 `Header/Tail` → `HEADER/TAIL`。
- **B3**:由「全链路弧度、仅注释错」**改写**为「ra/dec(度)与 `isInsideRaDecBounds`(<2π 弧度)单位不匹配」,保留(真实问题)。与复核方在此点存在分歧——复核方建议删除,本清单判断单位不匹配成立,改写保留并标注待确认设计意图。
- **B8/B9/B11/B14**:由中风险**降级**为「低优先级 · 防御性」。B8 补 `stopGrab` 不停 task worker;B9 撤销「返回 0」建议(有害);B11 确认当前不可触发;B14 确认正常契约不可达。
- **B13**:修复方向由「首帧上限 2000」改为「四帧检查 + 空间索引/低预算」(原方案不足)。
- **B15**:位置 `geo_validation.cpp` → `geo_continuous_tracking.cpp:16`;补充遗漏缺陷「仅有 ra/dec 时仍比 alpha/sigma」。
- **删除**「已确认安全」中关于回调/析构的错误推理。
- **C1–C7**:加「需 profiling/兼容性验证」声明。
- 文档整体由「修复清单」**降级**为「待核实清单」。

**2026-07-15(第三轮复核,外部 Codex 二次)**:在第二轮基础上再订正 7 处:
- **A1**:订正回调列举——`m_connections` 直接捕获 imageSender、trackDataStorage;imageProcessor、localImageStorage 在 frame source 内部 frame callback,**不在 m_connections**。停止顺序(先 connections 再 registry)不变。
- **A2**:措辞「实现定义的回绕」→「C++23 按模 2^32 的 well-defined 转换」。
- **B3**:方案由「二选一」改为「主推统一为弧度」——核实确认 `geo_association.cpp:225-244` 关联半径/最小运动/速度阈值均经 `arcsecToRad` 按弧度计算,链路已是弧度。
- **B4**:方案重写——部分写入不能 return(截断帧破坏对端同步、触发 A3);应循环写剩余或关闭重连;`flush()==false` 非必然失败。
- **B10**:方案简化——核实 `ImageSender` 已有单槽 pending 队列(`image_sender.h:98-105`),无需 AsyncWriteQueue;扩展 pending 携带 `imageFactory`,worker 线程执行。
- **B15**:方案修正——`onlyRaDec` 用零值判断不可靠(零是合法坐标);改为规定单一坐标表示或增加明确有效性标记。
- **「已确认安全」**:标题与措辞改为「本轮静态检查未发现」,避免把未压测结论写成已证实。

**2026-07-16(第四轮复核,外部 Codex 三次)**:补正 2 处:
- **B3**:补充 `isInsideRaDecBounds` 边界范围缺陷——`dec>0` 拒绝南天目标、`RA>0` 拒绝春分点;修正为赤纬 `[-π/2, π/2]`、赤经 `[0, 2π)`。原测试用 `dec=-0.32` 与旧 `>0` 边界矛盾,一并修正。
- **B15**:缩小修复范围——不「整个链路统一 alpha/sigma」(与 B3 确认的 RaDec 路径用 ra/dec 冲突),改为 `isSameEquatorialPoint` 只比较/接受 alpha/sigma,ra/dec 由 RaDec 链路单独处理。
