#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class ShadowBackend;

namespace ShadowLauncher {

class DebugLogger : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString logText READ logText NOTIFY logTextChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(int entryCount READ entryCount NOTIFY entryCountChanged)

public:
    explicit DebugLogger(QObject* parent = nullptr);

    QString logText() const { return m_logText; }
    bool visible() const { return m_visible; }
    void setVisible(bool v);
    int entryCount() const { return m_entries.size(); }

    Q_INVOKABLE void clear();
    Q_INVOKABLE void toggle();

public slots:
    void info(const QString& msg);
    void ok(const QString& msg);
    void warn(const QString& msg);
    void error(const QString& msg);
    void net(const QString& url, const QString& status, qint64 size, qint64 timeMs);
    void speed(qint64 bytesPerSec);

signals:
    void logTextChanged();
    void visibleChanged();
    void entryCountChanged();

private:
    void append(const QString& color, const QString& tag, const QString& msg);
    QString m_logText;
    QStringList m_entries;
    bool m_visible = false;
    static QString formatSize(qint64 bytes);
    static QString formatSpeed(qint64 bytesPerSec);
};

} // namespace ShadowLauncher
