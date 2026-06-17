#pragma once

#include <QObject>
#include <QString>

#include "dss/core/constants.h"
#include "dss/ui/view_model_context.h"

namespace Dss::Ui {

/**
 * @brief 处理子 ViewModel，负责处理模式状态与 ImageProcessor 策略同步。
 */
class ProcessingViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(int processingMode READ processingMode WRITE setProcessingMode NOTIFY
                   processingModeChanged)

public:
    /**
     * @brief 构造处理子 ViewModel。
     * @param context UI 子 ViewModel 共享的后端上下文。
     * @param parent Qt 父对象。
     */
    explicit ProcessingViewModel(UiServiceContext context, QObject* parent = nullptr);

    /**
     * @brief 析构处理子 ViewModel。
     */
    ~ProcessingViewModel() override;

    /**
     * @brief 获取当前处理模式。
     * @return 当前处理模式枚举的整型值。
     */
    [[nodiscard]] int processingMode() const;

public Q_SLOTS:
    /**
     * @brief 设置处理模式并同步到 ImageProcessor。
     * @param mode 处理模式枚举的整型值。
     */
    Q_INVOKABLE void setProcessingMode(int mode);

Q_SIGNALS:
    /**
     * @brief 处理模式变化。
     * @param mode 新处理模式枚举的整型值。
     */
    void processingModeChanged(int mode);

    /**
     * @brief 处理模块请求更新主状态栏文本。
     * @param text 状态栏文本。
     */
    void statusTextChanged(const QString& text);

private:
    /**
     * @brief 根据当前处理模式配置 ImageProcessor 策略。
     */
    void configureProcessingStrategy();

    Dss::Core::ServiceRegistry& m_registry;  ///< 应用服务注册表。
    int m_processingMode = static_cast<int>(Dss::Core::ProcessingMode::None);  ///< 当前处理模式。
};

}  // namespace Dss::Ui
