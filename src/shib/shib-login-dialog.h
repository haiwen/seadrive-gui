#ifndef SEAFILE_CLIENT_SHIB_LOGIN_DIALOG_H
#define SEAFILE_CLIENT_SHIB_LOGIN_DIALOG_H

#include <QDialog>
#include <QUrl>
#include <QNetworkCookieJar>
#include <QSslError>
#if !defined(SEAFILE_USE_WEBKIT)
#include <QWebEngineProfile>
#include <QWebEnginePage>
#endif

#include "account.h"

template<typename T> class QList;

#if defined(SEAFILE_USE_WEBKIT)
class QWebView;
#else
class QWebEngineView;
#endif

class QNetworkReply;
class QLineEdit;
class ApiError;
class FetchAccountInfoRequest;
class AccountInfo;

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
    ShibLoginDialog(const QUrl& url,
                    const QString& computer_name,
                    QWidget *parent=0);
    ~ShibLoginDialog();

    Account account() const { return account_; }

private slots:
    void sslErrorHandler(QNetworkReply* reply, const QList<QSslError> & ssl_errors);
    void onNewCookieCreated(const QUrl& url, const QNetworkCookie& cookie);
    void onWebEngineCookieAdded(const QNetworkCookie& cookie);
    void updateAddressBar(const QUrl& url);

private:
    Account parseAccount(const QString& txt);

private:
#if !defined(SEAFILE_USE_WEBKIT)
    QWebEngineProfile *web_engine_profile_;
    QWebEnginePage *web_engine_page_;
#endif


#if defined(SEAFILE_USE_WEBKIT)
    QWebView *webview_;
#else
    QWebEngineView *webview_;
#endif
    QUrl url_;
    QLineEdit *address_text_;
    bool cookie_seen_;

    Account account_;
};


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
