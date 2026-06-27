#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// Client for the privileged daemon (org.meridian.Store on the system bus).
// Calls are asynchronous: the daemon method only returns when the whole
// transaction is done, so a synchronous call would freeze the UI. Progress
// lines arrive as the `Progress` signal; the outcome as `Finished`.
class StoreDaemon : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString log READ log NOTIFY logChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
public:
    explicit StoreDaemon(QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    QString log() const { return m_log; }
    bool available() const { return m_available; }

    Q_INVOKABLE void install(const QStringList &packages);
    Q_INVOKABLE void remove(const QStringList &packages);
    Q_INVOKABLE void updateSystem();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void buildAur(const QString &pkgBase, const QString &rev);
    Q_INVOKABLE void clearLog();

Q_SIGNALS:
    void busyChanged();
    void logChanged();
    void finished(bool success, const QString &message);

private Q_SLOTS:
    void onProgress(const QString &line);
    void onFinished(bool success, const QString &message);

private:
    void callTransaction(const QString &method, const QVariantList &args);
    void setBusy(bool busy);
    void appendLog(const QString &line);

    bool m_busy = false;
    bool m_available = false;
    QString m_log;
};
