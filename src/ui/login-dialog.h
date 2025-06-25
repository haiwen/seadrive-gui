#ifndef SEAFILE_CLIENT_LOGIN_DIALOG_H
#define SEAFILE_CLIENT_LOGIN_DIALOG_H

#include <QDialog>
#include "ui_login-dialog.h"

#include <QUrl>
#include <QTimer>
#include <QString>
#include <QSslError>
#include <QNetworkReply>

class Account;
class LoginRequest;
class QNetworkReply;
class ApiError;
class FetchAccountInfoRequest;
class AccountInfo;
class ServerInfo;
class ClientSSOStatus;

class LoginDialog : public QDialog,
                    public Ui::LoginDialog
{
    Q_OBJECT
public:
    LoginDialog(QWidget *parent=0);
    void initFromAccount(const Account& account);

private slots:
    void toggleAdvancedOptions(bool checked);
    void fillSyncRootFolderName();
    void doLogin();
    void loginSuccess(const QString& token);
    void loginFailed(const ApiError& error);
    void loginWithShib();
    void onFetchAccountInfoSuccess(const AccountInfo& info);
    void onFetchAccountInfoFailed(const ApiError&);
    void serverInfoSuccess(const ServerInfo&);
    void serverInfoFailed(const ApiError&);
    void clientSSOLinkSuccess(const QString&);
    void clientSSOLinkFailed(const ApiError&);
    void checkClientSSOStatus();
    void clientSSOStatusSuccess(const ClientSSOStatus&);
    void clientSSOStatusFailed(const ApiError&);

private:
    Q_DISABLE_COPY(LoginDialog);

    enum LoginMode {
        LOGIN_NORMAL = 0,
        LOGIN_SHIB
    };

    void setupShibLoginLink();
    bool validateInputs();
    void disableInputs();
    void enableInputs();
    void showWarning(const QString& msg);

    void onNetworkError(const QNetworkReply::NetworkError& error, const QString& error_string);
    void onSslErrors(QNetworkReply *reply, const QList<QSslError>& errors);
    void onHttpError(int code);

    QUrl url_;
    QUrl sso_server_;
    QString username_;
    QString password_;
    QString sso_token_;
    QString computer_name_;
#ifdef Q_OS_WIN32
    QString sync_root_folder_name_;
#endif
    LoginRequest *request_;
    FetchAccountInfoRequest *account_info_req_;

    QString two_factor_auth_token_;
    QTimer client_sso_timer_;
    bool client_sso_success_;

    LoginMode mode_;
};

#endif // SEAFILE_CLIENT_LOGIN_DIALOG_H
