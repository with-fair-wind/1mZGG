#pragma once

#include <QObject>
#include <QString>
#include <cstddef>
#include <expected>

#include "dss/core/config.h"

namespace Dss::Ui {

/// @brief 设置页面可编辑字段的值快照。
struct SettingsSnapshot {
    QString dataRoot;                     ///< 数据根目录
    bool logEnabled = true;               ///< 是否启用文件日志
    QString logFilePath;                  ///< 日志文件路径
    std::size_t logMaxFileSizeBytes = 0;  ///< 单个日志文件最大字节数
    std::size_t logMaxFiles = 0;          ///< 保留的轮转日志文件数
    int imageWidth = 0;                   ///< 图像宽度
    int imageHeight = 0;                  ///< 图像高度
    double pixelScale = 0.0;              ///< 像元比例
};

/// @brief 在设置页面与全局配置对象之间转换并持久化参数。
class SettingsViewModel : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 创建设置 ViewModel。
     * @param config 待读写的全局配置对象。
     * @param parent Qt 父对象。
     */
    explicit SettingsViewModel(Dss::Core::Config& config = Dss::Core::Config::instance(),
                               QObject* parent = nullptr);

    /** @brief 获取界面设置快照。 @return 从当前配置转换得到的字段值。 */
    [[nodiscard]] auto snapshot() const -> SettingsSnapshot;

    /**
     * @brief 校验并保存设置。
     * @param settings 用户编辑后的字段值。
     * @return 保存成功时为空；校验或持久化失败时返回错误文本。
     */
    auto save(const SettingsSnapshot& settings) -> std::expected<void, QString>;

Q_SIGNALS:
    /// @brief 设置成功持久化。
    void settingsSaved();

    /**
     * @brief 请求更新主状态栏文本。
     * @param text 状态文本。
     */
    void statusTextChanged(const QString& text);

private:
    Dss::Core::Config& m_config;  ///< 共享的全局配置引用
};

}  // namespace Dss::Ui
