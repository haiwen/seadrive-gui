#ifndef SEADRIVE_CLIENT_OPEN_LOCAL_FILE_HELPER_H
#define SEADRIVE_CLIENT_OPEN_LOCAL_FILE_HELPER_H

#include <QObject>
#include <QUrl>
#include <QString>
class Account;

/**
 * Helper class to handle open local file request
 */
class OpenLocalHelper : public QObject
{
    Q_OBJECT
public:
    static OpenLocalHelper* instance();

    bool openLocalFile(const QUrl &url);

    void setUrl(const char *url) { url_ = url; }

    void handleOpenLocalFromCommandLine(const char *url);

    void checkPendingOpenLocalRequest();

private:
    static OpenLocalHelper* singleton_;

    OpenLocalHelper();

    void messageBox(const QString& msg);

    QByteArray url_;
};


#endif // SEADRIVE_CLIENT_OPEN_LOCAL_FILE_HELPER_H
