# 第三方软件许可说明

更新：2026-10-15。本项目自有代码的限制性许可见 [LICENSE](LICENSE)。该许可不覆盖或改变第三方代码及其原有授权，也不表示下列组件已取得商业许可。

本文件是主要依赖的许可索引，不是完整的依赖清单、许可证全文集合或发行合规证明。构建配置和 Conan 解析结果决定实际依赖；分发时仍须按具体组件条款提供对应版本的完整许可、版权声明、NOTICE、相应源代码或其他必要材料。

## 主要构建依赖

| 组件 | 当前声明版本或来源 | 许可与边界 | 上游来源 |
|---|---|---|---|
| Qt Core、Gui、Widgets、Network、SerialPort、OpenGL、OpenGLWidgets | 本机所选 Qt SDK；CI 基线为 6.8.0 | 按具体模块和 SDK 的商业或开源许可；采用 LGPLv3 时，须保留其授予的权利并履行相应义务 | [Qt 许可说明](https://doc.qt.io/qt-6/licensing.html) |
| Qt Charts | 当前 CMake 声明的 Qt 模块 | 商业许可或 GPLv3，不是 LGPL；不得依据项目自有许可对 GPL 覆盖的组合附加非商业或禁止收费限制 | [Qt Charts 许可](https://doc.qt.io/qt-6.8/qtcharts-index.html#licenses) |
| spdlog | `spdlog/1.14.0` | MIT；保留原版权及许可声明 | [1.14.0 LICENSE](https://github.com/gabime/spdlog/blob/v1.14.0/LICENSE) |
| nlohmann/json | `nlohmann_json/3.11.3` | MIT；保留原版权及许可声明 | [3.11.3 LICENSE.MIT](https://github.com/nlohmann/json/blob/v3.11.3/LICENSE.MIT) |
| OpenCV | 可选 `opencv/4.10.0` | Apache-2.0；随附第三方代码及间接依赖另按各自条款处理 | [4.10.0 LICENSE](https://github.com/opencv/opencv/blob/4.10.0/LICENSE) |
| GoogleTest | `gtest/1.15.0`，测试依赖 | BSD-3-Clause；若分发测试程序或相关源码，保留原声明并满足其条件 | [1.15.0 LICENSE](https://github.com/google/googletest/blob/v1.15.0/LICENSE) |
| fmt | spdlog 的间接依赖；当前本机解析为 10.2.1，其他配置以 Conan 结果为准 | MIT，含上游可选的嵌入对象代码例外；以实际版本文本为准 | [10.2.1 LICENSE](https://github.com/fmtlib/fmt/blob/10.2.1/LICENSE) |

Qt 模块内部、OpenCV 的平台依赖、编译器运行库及部署工具带入的运行时组件，均须根据实际发行包核对；不能以此表代替它们的原始许可。

## 可选或外部组件

| 组件 | 接入方式 | 许可边界 |
|---|---|---|
| ElaWidgetTools | 本地提供 `third_party/ElaWidgetTools` 时启用；当前检视目录未提供该组件 | [上游当前 LICENSE](https://github.com/Liniyous/ElaWidgetTools/blob/main/LICENSE) 为 MIT；接入时以实际提交、分支及随附字体、图标等资源的授权为准 |
| NVIDIA CUDA Toolkit / Runtime | `DSS_ENABLE_CUDA` 显式启用 | 依据安装版本的 NVIDIA SDK/EULA 和可再分发组件清单；本项目许可不提供任何 NVIDIA 授权 |
| Teledyne Sapera SDK | `DSS_ENABLE_SAPERA` 显式启用，本机 SDK | 按供应商提供的 SDK 合同和 EULA 确认使用、链接及再分发权利；本项目未作统一许可声明 |
| StarMap、Photometry、StarPosition | `DSS_ENABLE_STARLIBS` 启用时从外部 `proLib` 查找 | 本项目未确认这些库的权利归属与发行许可；不得仅凭存在本项目许可即认定可以分发 |
| `oldsrc/` 归档及其他来源片段 | 不参与默认构建 | 原版权、第三方许可和既有授权继续有效；来源或授权未确认的部分不得自动视为 kk 可重新许可的原创代码 |

## 当前发行边界

当前 CMake 仍声明并链接 `Qt6::Charts`；本轮添加自有许可不调整该依赖。当前生产源码未检索到图表 API 的实际调用，相关调用位于 `oldsrc/`，现代 UI 保留了待实现提示。真正交付前应检查实际链接与打包结果；如组合受 GPLv3 覆盖，须先移除或替换相关依赖、取得适当商业授权，或采用其他权利人允许的兼容方案。

构建时把本项目 `LICENSE` 和本索引复制到运行目录，仅保证自有许可说明随产物携带，不表示已完成全部第三方声明、源码提供或商业授权义务。许可文件不会自动将 GitHub 仓库或 CI 产物变为私有；访问控制仍需由仓库和平台设置实现。
