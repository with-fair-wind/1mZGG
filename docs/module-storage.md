# Storage 模块

> 命名空间: 包含于 `Dss::Storage`
>
> 头文件: `include/dss/storage/`
>
> 源文件: 无独立编译目标（header-only + 轻量后端）

## 模块职责

Storage 模块定义图像和轨迹数据的存储格式，以及本地存储后端。当前以 header-only 格式定义为主，异步写入工作线程尚未实现。

## 组件清单

### 1. IStorageBackend (`i_storage_backend.h`)

存储后端抽象接口：

```cpp
class IStorageBackend : public IService {
    virtual auto init(basePath) -> bool = 0;
};
```

### 2. LocalImageStorageBackend (`local_image_storage_backend.h`)

本地图像存储后端，当前仅持有 `baseDir` 路径，无异步写入实现。

### 3. TrackDataStorageBackend (`track_data_storage_backend.h`)

轨迹数据存储后端，当前仅持有 `baseDir` 路径，无异步写入实现。

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
| GAE 文件 | 方位/俯仰数据文件 |

## 旧版对照

| 旧版 | 新版 | 状态 |
|------|------|------|
| `ImageCode.h/.cpp` (RAW编解码) | `image_storage_format.h` | 格式定义已迁移 |
| `ImageCode.h/.cpp` (BMP编解码) | `bmp_image_format.h` | 格式定义已迁移 |
| `ImageStorage.h/.cpp` (文件I/O) | `LocalImageStorageBackend` | 仅路径持有，无写入 |
| `TrackDataStorage.h/.cpp` | `TrackDataStorageBackend` + format | 仅格式定义，无I/O |
| `ImageReplayer.h/.cpp` | — | **未迁移** |

## 当前缺口

| 缺口 | 说明 |
|------|------|
| 异步写入 | 无工作线程实现帧数据的异步磁盘写入 |
| 回放功能 | `ImageReplayer` 完全未迁移 |
| 存储后端 | `LocalImageStorageBackend` / `TrackDataStorageBackend` 仅为桩实现 |

## 依赖关系

Storage 模块的格式定义为 header-only，仅依赖 `dss_core` 类型。后端通过 `ServiceRegistry` 注册到 App 模块。
