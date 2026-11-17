#ifndef SEADRIVE_CLIENT_OPEN_LOCAL_FILE_HELPER_H
#define SEADRIVE_CLIENT_OPEN_LOCAL_FILE_HELPER_H

#include <QObject>
#include <QUrl>
#include <QString>
#include <QList>
class Account;

/**
 * Helper class to handle open local file request
 */
class OpenLocalHelper : public QObject
{
    Q_OBJECT

public:
    static OpenLocalHelper* instance();

    void setUrl(const QString &url);

    void handleOpenLocalFromCommandLine(const QString &url);

    void checkPendingOpenLocalRequest();

public slots:
    bool openLocalFile(const QUrl &url);

private:
    static OpenLocalHelper* singleton_;

    OpenLocalHelper();
    ~OpenLocalHelper();

    void messageBox(const QString& msg);

    bool dispatch(const QUrl &url, bool *pending);
#ifdef Q_OS_MACOS
    QString readiness() const;
    QString last_readiness_;
#endif
    QList<QUrl> pending_urls_;
};


#endif // SEADRIVE_CLIENT_OPEN_LOCAL_FILE_HELPER_H
