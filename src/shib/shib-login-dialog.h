#ifndef SEAFILE_CLIENT_SHIB_LOGIN_DIALOG_H
#define SEAFILE_CLIENT_SHIB_LOGIN_DIALOG_H

#include <QDialog>
#include <QUrl>
#include <QNetworkCookieJar>

#if defined(SEAFILE_USE_WEBKIT)
  #include <QWebPage>
#endif

#include "account.h"

template<typename T> class QList;

#if defined(SEAFILE_USE_WEBKIT)
class QWebView;
#else
class QWebEngineView;
#endif

class QSslError;
class QNetworkReply;
class QLineEdit;

class ApiError;
class FetchAccountInfoRequest;

/**
 * Login with Shibboleth SSO.
 *
 * This dialog use a webview to let the user login seahub configured with
 * Shibboleth SSO auth. When the login succeeded, seahub would set the
 * username and api token in the cookie.
 */
class ShibLoginDialog : public QDialog {
    Q_OBJECT
public:
    ShibLoginDialog(QWidget *parent=0);

    Account account() const { return account_; }

private slots:
    void sslErrorHandler(QNetworkReply* reply, const QList<QSslError> & ssl_errors);
    void onNewCookieCreated(const QUrl& url, const QNetworkCookie& cookie);
    void onWebEngineCookieAdded(const QNetworkCookie& cookie);
    void updateAddressBar(const QUrl& url);
    void onFetchAccountInfoSuccess(const AccountInfo& info);
    void onFetchAccountInfoFailed(const ApiError&);

private:
    Account parseAccount(const QString& txt);
    void getAccountInfo(const Account& account);

#if defined(SEAFILE_USE_WEBKIT)
    QWebView *webview_;
#else
    QWebEngineView *webview_;
#endif
    QUrl url_;
    QLineEdit *address_text_;
    bool cookie_seen_;
    FetchAccountInfoRequest *account_info_req_;
    Account account_;
};

#if defined(SEAFILE_USE_WEBKIT)
class SeafileWebPage : public QWebPage {
    Q_OBJECT
public:
    SeafileWebPage(QObject *parent = 0);

protected:
    virtual void javaScriptConsoleMessage(const QString &message,
                                          int lineNumber,
                                          const QString &sourceID);
};
#endif

/**
 * Wraps the standard Qt cookie jar to emit a signal when new cookies created.
 */
class CustomCookieJar : public QNetworkCookieJar
{
    Q_OBJECT
public:
    explicit CustomCookieJar(QObject *parent = 0);
    bool setCookiesFromUrl(const QList<QNetworkCookie>& cookies, const QUrl& url);

signals:
    void newCookieCreated(const QUrl& url, const QNetworkCookie& cookie);
};

#endif /* SEAFILE_CLIENT_SHIB_LOGIN_DIALOG_H */
