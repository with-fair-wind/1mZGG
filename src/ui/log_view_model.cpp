#include "dss/ui/log_view_model.h"

#include <utility>

namespace Dss::Ui {
namespace {

/// @brief 构造网络发送失败的状态栏文本。
[[nodiscard]] auto makeNetworkTransmissionErrorStatus(
    const Dss::Core::NetworkTransmissionErrorEvent& event) -> QString {
    return QString("Network send failed (%1): %2")
        .arg(QString::fromStdString(event.channel))
        .arg(QString::fromStdString(event.message));
}

/// @brief 构造网络发送失败的日志文本。
[[nodiscard]] auto makeNetworkTransmissionErrorLog(
    const Dss::Core::NetworkTransmissionErrorEvent& event) -> QString {
    return QString("[ERROR] %1 | attempted bytes: %2")
        .arg(makeNetworkTransmissionErrorStatus(event))
        .arg(event.attemptedBytes);
}

/// @brief 构造串口帧校验失败的日志文本。
[[nodiscard]] auto makeSerialFrameErrorLog(const Dss::Core::SerialFrameErrorEvent& event)
    -> QString {
    return QString(
               "[WARN] Serial frame dropped (%1): %2 | expected bytes: %3 actual bytes: %4 "
               "header: %5 tail: %6")
        .arg(QString::fromStdString(event.channel))
        .arg(QString::fromStdString(event.message))
        .arg(event.expectedBytes)
        .arg(event.actualBytes)
        .arg(static_cast<unsigned int>(event.observedHeader))
        .arg(static_cast<unsigned int>(event.observedTail));
}

/// @brief 构造串口字段级解码失败的日志文本。
[[nodiscard]] auto makeSerialDecodeErrorLog(const Dss::Core::SerialDecodeErrorEvent& event)
    -> QString {
    return QString("[WARN] Serial decode failed (%1): %2 | field: %3 offset: %4 raw: %5")
        .arg(QString::fromStdString(event.channel))
        .arg(QString::fromStdString(event.message))
        .arg(QString::fromStdString(event.field))
        .arg(event.byteOffset)
        .arg(event.rawValue);
}

/// @brief 将 UI 整数值转换为日志级别。
[[nodiscard]] auto logLevelFromInt(int level) -> Dss::Core::LogLevel {
    if (level <= static_cast<int>(Dss::Core::LogLevel::Info)) {
        return Dss::Core::LogLevel::Info;
    }
    if (level >= static_cast<int>(Dss::Core::LogLevel::Error)) {
        return Dss::Core::LogLevel::Error;
    }
    return Dss::Core::LogLevel::Warning;
}

}  // namespace

LogViewModel::LogViewModel(UiServiceContext context, QObject* parent)
    : QObject(parent), m_bus(context.bus) {
    setupSubscriptions();
}

LogViewModel::~LogViewModel() = default;

auto LogViewModel::logMinimumLevel() const -> int {
    return static_cast<int>(m_logMinimumLevel);
}

auto LogViewModel::visibleLogEntries() const -> QStringList {
    QStringList entries;
    entries.reserve(static_cast<qsizetype>(m_logEntries.size()));
    for (const auto& entry : m_logEntries) {
        if (isLogLevelVisible(entry.level)) {
            entries.push_back(entry.text);
        }
    }
    return entries;
}

void LogViewModel::setLogMinimumLevel(int level) {
    const auto normalizedLevel = logLevelFromInt(level);
    if (m_logMinimumLevel == normalizedLevel) {
        return;
    }

    m_logMinimumLevel = normalizedLevel;
    Q_EMIT logMinimumLevelChanged(logMinimumLevel());
    Q_EMIT logEntriesChanged(visibleLogEntries());
}

void LogViewModel::setupSubscriptions() {
    m_connections.push_back(m_bus.subscribe<Dss::Core::NetworkTransmissionErrorEvent>(
        [this](const Dss::Core::NetworkTransmissionErrorEvent& e) {
            onNetworkTransmissionError(e);
        }));

    m_connections.push_back(m_bus.subscribe<Dss::Core::SerialFrameErrorEvent>(
        [this](const Dss::Core::SerialFrameErrorEvent& e) { onSerialFrameError(e); }));

    m_connections.push_back(m_bus.subscribe<Dss::Core::SerialDecodeErrorEvent>(
        [this](const Dss::Core::SerialDecodeErrorEvent& e) { onSerialDecodeError(e); }));

    m_connections.push_back(m_bus.subscribe<Dss::Core::LogMessageEvent>(
        [this](const Dss::Core::LogMessageEvent& e) { onLogMessage(e); }));
}

void LogViewModel::onNetworkTransmissionError(
    const Dss::Core::NetworkTransmissionErrorEvent& event) {
    Q_EMIT statusTextChanged(makeNetworkTransmissionErrorStatus(event));
    appendLogEntry(Dss::Core::LogLevel::Error, makeNetworkTransmissionErrorLog(event));
}

void LogViewModel::onSerialFrameError(const Dss::Core::SerialFrameErrorEvent& event) {
    appendLogEntry(Dss::Core::LogLevel::Warning, makeSerialFrameErrorLog(event));
}

void LogViewModel::onSerialDecodeError(const Dss::Core::SerialDecodeErrorEvent& event) {
    appendLogEntry(Dss::Core::LogLevel::Warning, makeSerialDecodeErrorLog(event));
}

void LogViewModel::onLogMessage(const Dss::Core::LogMessageEvent& event) {
    appendLogEntry(event.level, QString::fromStdString(event.message));
}

void LogViewModel::appendLogEntry(Dss::Core::LogLevel level, QString text) {
    if (m_logEntries.size() == kMaxLogEntries) {
        m_logEntries.erase(m_logEntries.begin());
    }

    m_logEntries.push_back(UiLogEntry{.level = level, .text = std::move(text)});
    const auto& entry = m_logEntries.back();
    if (isLogLevelVisible(entry.level)) {
        Q_EMIT logEntryAppended(entry.text);
    }
    Q_EMIT logEntriesChanged(visibleLogEntries());
}

auto LogViewModel::isLogLevelVisible(Dss::Core::LogLevel level) const -> bool {
    return static_cast<int>(level) >= static_cast<int>(m_logMinimumLevel);
}

}  // namespace Dss::Ui
