#pragma once

#include <QObject>
#include <QString>
#include <optional>
#include <thread>

#include "dss/storage/format/image_storage_format.h"
#include "dss/ui/view_model/view_model_context.h"

namespace Dss::Ui {

/**
 * @brief 存储子 ViewModel，负责图像和轨迹存储 worker 的启动/停止。
 */
class StorageViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isSaving READ isSaving NOTIFY savingChanged)
    Q_PROPERTY(bool isStopping READ isStopping NOTIFY stoppingChanged)

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

    /** @brief 查询异步存储停止状态。 @return 正在排空时为 true。 */
    [[nodiscard]] bool isStopping() const {
        return m_stopping;  ///< 正在等待后台存储排空。
    }
    /// 关闭应用时等待排空完成，并取消尚未开始的新会话。
    void shutdown();

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
    /** @brief 异步停止状态变化。 @param value 是否正在排空。 */
    void stoppingChanged(bool value);
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
    /// @brief 在对象线程完成停止状态更新，并按需启动待处理会话。
    void finishStop();

    Dss::Core::ServiceRegistry& m_registry;  ///< 应用服务注册表。
    bool m_saving = false;                   ///< 是否正在保存。
    bool m_stopping = false;                 ///< 正在等待后台存储排空。
    bool m_shutdown = false;                 ///< 已关闭，忽略后续异步完成通知。
    std::optional<Dss::Storage::ImageStorageNaming>
        m_pendingSession;       ///< 排空完成后待启动的会话；停止命令可取消。
    std::jthread m_stopWorker;  ///< 仅执行后端排空；完成通知在 QObject 线程处理。
};

}  // namespace Dss::Ui
