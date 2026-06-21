# Storage 模块

> 命名空间: 包含于 `Dss::Storage`
>
> 头文件: `include/dss/storage/`
>
> 源文件: 无独立编译目标（header-only + 轻量后端）

## 模块职责

Storage 模块定义图像和轨迹数据的存储格式、本地异步后端与会话命名。RAW/BMP、IMI/GAE 会话文件、背压统计、错误事件和主控任务到会话命名的自动映射均已接入。

## 组件清单

### 1. IStorageBackend (`i_storage_backend.h`)

存储后端抽象接口：

```cpp
class IStorageBackend : public IService {
    virtual auto init(basePath) -> bool = 0;
};
```

### 2. LocalImageStorageBackend (`local_image_storage_backend.h`)

本地图像存储后端，持有 `baseDir` 路径，并提供显式启动的 I/O worker：

| 方法 | 说明 |
|------|------|
| `init(baseDir)` | 创建/设置存储根目录 |
| `start()` / `stop()` | 启动/停止后台写入线程，停止时 drain 队列 |
| `enqueueRawFrame(path, metadata, pixels)` | 入队 legacy RAW 帧写入 |
| `isRunning()` | 查询 worker 状态 |

### 3. TrackDataStorageBackend (`track_data_storage_backend.h`)

轨迹数据存储后端，持有 `baseDir` 路径，并提供显式启动的 I/O worker：

| 方法 | 说明 |
|------|------|
| `init(baseDir)` | 创建/设置轨迹数据目录 |
| `start()` / `stop()` | 启动/停止后台写入线程，停止时 drain 队列 |
| `enqueueTrackResult(event)` | 将 `TrackResultEvent` 先归一为 `ResultPacket`，再转为 legacy 轨迹文本记录并入队 |
| `outputPath()` | 返回当前 `track_data.txt` 输出路径 |
| `isRunning()` | 查询 worker 状态 |

### 4. 图像存储格式 (`image_storage_format.h`)

定义 RAW 文件和命名规则，从旧版 `ImageCode` + `ImageStorage` 迁移。

| 内容 | 说明 |
|------|------|
| RAW 文件头 | 31 字节，包含帧序号、尺寸、时间戳等元数据 |
| 文件命名规则 | 基于时间戳和目标 ID 生成文件名 |
| IFM 内容格式 | 图像文件索引清单 |
| 回放间隔 | `replayIntervalMilliseconds()` 计算回放帧间隔 |

### 5. BMP 图像格式 (`bmp_image_format.h`)

旧版 BMP 文件格式兼容，从 `ImageCode` BMP 路径迁移。

| 内容 | 说明 |
|------|------|
| BMP 头 | 标准 Windows BMP 头 |
| 属性头 | 60 字节自定义属性区 |

### 6. 轨迹数据格式 (`track_data_storage_format.h`)

旧版轨迹数据文本格式，从 `TrackDataStorage` 迁移。

| 内容 | 说明 |
|------|------|
| 行格式 | 每行一帧的跟踪结果文本表示 |
| `makeTrackDataRecord(packet)` | 从通用 `ResultPacket` 构造 legacy 文本记录，复用同一结果 DTO |
| GAE 文件 | 方位/俯仰数据文件 |

## 旧版对照

| 旧版 | 新版 | 状态 |
|------|------|------|
| `ImageCode.h/.cpp` (RAW编解码) | `image_storage_format.h` | 格式定义已迁移 |
| `ImageCode.h/.cpp` (BMP编解码) | `bmp_image_format.h` | 格式定义已迁移 |
| `ImageStorage.h/.cpp` (文件I/O) | `LocalImageStorageBackend` | RAW/BMP 异步写入、IMI 会话索引、背压和错误事件已迁移 |
| `TrackDataStorage.h/.cpp` | `TrackDataStorageBackend` + format | `track_data.txt`/GAE 会话写入、结果归一化、背压和错误事件已迁移 |
| `ImageReplayer.h/.cpp` | `ImageSequenceFrameSource` | 回放读取已迁至 Acquisition 模块 |

## 当前缺口

| 缺口 | 说明 |
|------|------|
| 产品级保留策略 | 当前已具备会话级写入与背压；配额、清理周期和归档策略属于后续产品配置，不是 legacy 迁移阻塞项 |

## 依赖关系

Storage 模块的格式定义为 header-only，仅依赖 `dss_core` 类型。后端通过 `ServiceRegistry` 注册到 App 模块。
