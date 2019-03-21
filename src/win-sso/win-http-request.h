#ifndef SEADRIVE_GUI_FILE_BROWSER_DIALOG_H
#define SEADRIVE_GUI_FILE_BROWSER_DIALOG_H

#include <QString>
#include <QUrl>
#include <QStringList>

#include <windows.h>
#include <wininet.h>

#if !defined(INTERNET_COOKIE_HTTPONLY)
#define INTERNET_COOKIE_HTTPONLY 0x00002000
#endif


class WinInetHttpResponse;

// Encapsulates a http request sending with WinInet library. WinInet can
// handle automatically NTLM/Kerberos authentication using the credentials of
// the currently logged-on user when the server asks for it.
class WinInetHttpReqest
{
public:
    WinInetHttpReqest(const QUrl& url);

    bool send(WinInetHttpResponse* response);

    bool getCookie(const char* cookie_name, QString* cookie_value);

    static void CALLBACK statusCallback(HINTERNET hInternet,
                                        DWORD_PTR dwContext,
                                        DWORD     dwInternetStatus,
                                        LPVOID    lpvStatusInformation,
                                        DWORD     dwStatusInformationLength);

private:
    QString cookieUrl() const;
    void flushSessionCookies();
    bool listCookies(QStringList *cookies);
    bool getCookiePlain(const char* cookie_name, QString* cookie_value);
    bool getCookieProtected(const char* cookie_name, QString* cookie_value);

    QUrl url_;
    QUrl redirected_url_;
};

class WinInetHttpResponse
{
public:
    WinInetHttpResponse(int code = 0, const QString& body = QString())
        : code_(code), body_(body)
    {
    }

private:
    int code_;
    QString body_;
};


#endif // SEADRIVE_GUI_FILE_BROWSER_DIALOG_H
