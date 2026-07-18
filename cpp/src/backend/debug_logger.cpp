#include "debug_logger.h"
#include <QDateTime>
#include <QDebug>

namespace ShadowLauncher {

DebugLogger::DebugLogger(QObject* parent) : QObject(parent) {}

void DebugLogger::setVisible(bool v)
{
    if (m_visible != v) { m_visible = v; emit visibleChanged(); }
}

void DebugLogger::toggle()
{
    setVisible(!m_visible);
}

void DebugLogger::clear()
{
    m_logText.clear();
    m_entries.clear();
    emit logTextChanged();
    emit entryCountChanged();
}

QString DebugLogger::formatSize(qint64 bytes)
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024LL * 1024 * 1024) return QString::number(bytes / 1024.0 / 1024.0, 'f', 1) + " MB";
    return QString::number(bytes / 1024.0 / 1024.0 / 1024.0, 'f', 2) + " GB";
}

QString DebugLogger::formatSpeed(qint64 bytesPerSec)
{
    return formatSize(bytesPerSec) + "/s";
}

void DebugLogger::append(const QString& color, const QString& tag, const QString& msg)
{
    QString ts = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    QString line = QString("<span style='color:#888'>[%1]</span> <span style='color:%2'>[%3]</span> %4<br>")
        .arg(ts, color, tag, msg.toHtmlEscaped());
    m_entries.append(line);
    m_logText += line;
    // Keep max 2000 lines
    while (m_entries.size() > 2000) {
        m_entries.removeFirst();
        int idx = m_logText.indexOf("<br>");
        if (idx >= 0) m_logText = m_logText.mid(idx + 4);
    }
    emit logTextChanged();
    emit entryCountChanged();
}

void DebugLogger::info(const QString& msg)
{
    append("#aaa", "信息", msg);
}

void DebugLogger::ok(const QString& msg)
{
    append("#4caf50", "完成", msg);
}

void DebugLogger::warn(const QString& msg)
{
    append("#ff9800", "警告", msg);
}

void DebugLogger::error(const QString& msg)
{
    append("#f44336", "错误", msg);
}

void DebugLogger::net(const QString& url, const QString& status, qint64 size, qint64 timeMs)
{
    QString detail;
    if (status == "OK") {
        QString speedStr = timeMs > 0 ? formatSpeed(size * 1000 / timeMs) : "—";
        detail = QString("[完成] %1  %2  %3ms  <span style='color:#888'>%4</span>")
            .arg(formatSize(size), speedStr)
            .arg(timeMs)
            .arg(url);
        append("#4caf50", "网络", detail);
    } else {
        detail = QString("[失败] %1ms  %2  <span style='color:#888'>%3</span>")
            .arg(timeMs)
            .arg(status)
            .arg(url);
        append("#f44336", "网络", detail);
    }
}

void DebugLogger::speed(qint64 bytesPerSec)
{
    // Speed is too frequent for text log — just store latest
    Q_UNUSED(bytesPerSec);
}

} // namespace ShadowLauncher
