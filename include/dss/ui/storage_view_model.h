#pragma once

#include <QObject>
#include <QString>

#include "dss/storage/image_storage_format.h"
#include "dss/ui/view_model_context.h"

namespace Dss::Ui {

/**
 * @brief 存储子 ViewModel，负责图像和轨迹存储 worker 的启动/停止。
 */
class StorageViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isSaving READ isSaving NOTIFY savingChanged)

public:
    /**
     * @brief 构造存储子 ViewModel。
     * @param context UI 子 ViewModel 共享的后端上下文。
     * @param parent Qt 父对象。
     */
    explicit StorageViewModel(UiServiceContext context, QObject* parent = nullptr);

    /**
     * @brief 析构存储子 ViewModel。
     */
    ~StorageViewModel() override;

    /**
     * @brief 查询当前是否正在保存。
     * @return 正在保存时返回 true。
     */
    [[nodiscard]] bool isSaving() const;

    /**
     * @brief 使用显式业务会话命名启动存储。
     * @param naming 图像和跟踪数据共享的会话命名信息。
     */
    void startSaving(const Dss::Storage::ImageStorageNaming& naming);

public Q_SLOTS:
    /**
     * @brief 启动图像与跟踪数据本地存储。
     */
    Q_INVOKABLE void startSaving();

    /**
     * @brief 停止图像与跟踪数据本地存储。
     */
    Q_INVOKABLE void stopSaving();

Q_SIGNALS:
    /**
     * @brief 保存状态变化。
     * @param value 新保存状态。
     */
    void savingChanged(bool value);

    /**
     * @brief 存储模块请求更新主状态栏文本。
     * @param text 状态栏文本。
     */
    void statusTextChanged(const QString& text);

private:
    /**
     * @brief 更新保存状态并发出变化信号。
     * @param value 新保存状态。
     */
    void setSaving(bool value);

    Dss::Core::ServiceRegistry& m_registry;  ///< 应用服务注册表。
    bool m_saving = false;                   ///< 是否正在保存。
};

}  // namespace Dss::Ui
