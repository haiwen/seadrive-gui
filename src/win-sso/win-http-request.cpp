#if !defined(WINVER)
#define WINVER 0x0501
#endif

#include <inttypes.h>
#include <urlmon.h>
#include <cstdio>
#include <string>

#include <QStringList>
#include <QtDebug>

#include <windows.h>
#include <wininet.h>

#include "win-http-request.h"

namespace
{
DWORD kQueryCookieFlag = INTERNET_COOKIE_HTTPONLY;
// DWORD kQueryCookieFlag = 0;

std::string formatErrorMessage()
{
    DWORD error_code = ::GetLastError();
    if (error_code == 0) {
        return "no error";
    }
    char buf[256] = {0};
    ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error_code,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf,
                    sizeof(buf) - 1, NULL);
    return buf;
}

void reportError(const char* msg)
{
    qWarning("%s: %s\n", msg, formatErrorMessage().c_str());
}

} // namespace

WinInetHttpReqest::WinInetHttpReqest(const QUrl& url) : url_(url)
{
}

static int32_t readStatusCode(HINTERNET request)
{
    // char buf[65536] = {0};
    // DWORD bufsize = (DWORD)sizeof(buf) - 1;
    DWORD index = 0;
    int32_t code = 0;
    DWORD bufsize = sizeof(code);
    if (!HttpQueryInfo(request, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
                       &code, &bufsize, &index)) {
        return false;
    }

    qDebug("status code is %d\n", code);

    return code;
}

static bool readHeaders(HINTERNET request, QString* headers)
{
    char buf[65536] = {0};
    DWORD bufsize = (DWORD)sizeof(buf) - 1;
    DWORD index = 0;
    if (!HttpQueryInfo(request, HTTP_QUERY_RAW_HEADERS_CRLF, buf, &bufsize,
                       &index)) {
        return false;
    }

    qDebug("raw headers:\n");
    qDebug("%s\n", buf);

    *headers = QString::fromUtf8(buf);
    return true;
}

bool WinInetHttpReqest::listCookies(QStringList* cookies)
{
    char* buf = NULL;
    DWORD len = 0;
    BOOL success = false;
    char url[4096] = {0};
    snprintf(url, sizeof(url), "%s", url_.toString().toUtf8().data());

    success = InternetGetCookieExA(url, NULL, NULL, &len, kQueryCookieFlag, 0);
    if (!success) {
        qWarning("failed to get cookie length of %s, GLE=%lu\n", url,
                 GetLastError());
        return false;
    }
    buf = (char*)malloc((size_t)(len + 1));
    buf[len] = 0;
    success = InternetGetCookieExA(url, NULL, buf, &len, kQueryCookieFlag, 0);

    if (!success) {
        qWarning("failed to get cookie of %s, GLE=%lu\n", url, GetLastError());
        return false;
    }

    qDebug("cookie of %s = %s\n\n", url, buf);

    QString str = QString::fromUtf8(buf).trimmed();
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    QStringList sections = str.split(";", Qt::SkipEmptyParts);
#else
    QStringList sections = str.split(";", QString::SkipEmptyParts);
#endif
    foreach (const QString& section, sections) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
        QStringList parts = section.trimmed().split("=", Qt::SkipEmptyParts);
#else
        QStringList parts = section.trimmed().split("=", QString::SkipEmptyParts);
#endif
        if (parts.size() > 1) {
            cookies->push_back(parts[0].trimmed());
        }
    }
    return true;
}

/**
 * WinInet API shares the IE cookies, so if the user has already logged in
 * seafile on IE, the request would be accepted by nginx, so that the krb5
 * middleware on apache won't have a chance to add the `seahub_auth` to the
 * cookie.
 *
 * To avoid this, we clear the IE session cookie before sending the request.
 */
void WinInetHttpReqest::flushSessionCookies()
{
    QStringList cookies;
    if (!listCookies(&cookies)) {
        qDebug("no existing cookies for %s", url_.toString().toUtf8().data());
        // return false;
    }

    foreach (const QString& cookie, cookies) {
        if (cookie == "csrftoken") {
            continue;
        }
        qDebug("flusing cookie %s", cookie.toUtf8().data());
        // flush session cookies.
        InternetSetCookieExA(url_.toString().toUtf8().data(), // url
                             cookie.toUtf8().data(), // cookie name
                             "", // cookie value
                             INTERNET_COOKIE_HTTPONLY, 0);
    }
}

void CALLBACK WinInetHttpReqest::statusCallback(HINTERNET hInternet,
                                                DWORD_PTR dwContext,
                                                DWORD     dwInternetStatus,
                                                LPVOID    lpvStatusInformation,
                                                DWORD     dwStatusInformationLength)
{
    // qWarning("statusCallback called with dwInternetStatus = %lu", dwInternetStatus);

    // We only care about the redirect event
    if (dwInternetStatus != INTERNET_STATUS_REDIRECT) {
        return;
    }

    qDebug("redirected to %s", (char *)lpvStatusInformation);

    WinInetHttpReqest *request = (WinInetHttpReqest *)dwContext;

    QUrl new_url(QString::fromLatin1((char *)lpvStatusInformation));

    if (new_url.host() != request->url_.host()) {
        qWarning() << "redirected to a diffrent domain" << request->url_.host()
                   << "=>" << new_url.host();
    }
    request->redirected_url_ = new_url;
}


bool WinInetHttpReqest::send(WinInetHttpResponse* response)
{
    // Retrieve default http user agent
    char user_agent[4096];
    DWORD ua_size = sizeof(user_agent);
    ObtainUserAgentString(0, user_agent, &ua_size);

    char* header = NULL;
    char* data = NULL;
    QString headers;
    DWORD body_size = 0;
    char buf[65536] = {0};
    DWORD bytes_read = 0;
    int code;
    QString u;
    QString query;

    HINTERNET connection = NULL;
    HINTERNET session = NULL;
    HINTERNET request = NULL;
    bool ret = false;

    flushSessionCookies();

    connection = InternetOpenA(user_agent, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (connection == NULL) {
        reportError("InternetOpenA failed");
        goto out;
    }

    session = InternetConnectA(
        connection,
        url_.host().toUtf8().data(),
        url_.port(INTERNET_DEFAULT_HTTP_PORT),
        NULL,
        NULL,
        INTERNET_SERVICE_HTTP,
        0,
        (DWORD_PTR) this);

    if (session == NULL) {
        reportError("InternetConnectA failed");
        goto out;
    }

    // We need to register a status callback function so we can get notified
    // when redirects happen, so that we can retrieve the cookie for the correct
    // domain in the end.
    InternetSetStatusCallback(session, (INTERNET_STATUS_CALLBACK)WinInetHttpReqest::statusCallback);

    u = url_.toString();
    query = u.mid(u.indexOf('?'));
    request = HttpOpenRequestA(
        session, "GET", (url_.path() + query).toUtf8().data(), "HTTP/1.1", NULL,
        NULL, INTERNET_FLAG_HYPERLINK | INTERNET_FLAG_IGNORE_CERT_CN_INVALID |
                  INTERNET_FLAG_IGNORE_CERT_DATE_INVALID |
                  INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTP |
                  INTERNET_FLAG_IGNORE_REDIRECT_TO_HTTPS |
                  INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_UI |
                  INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_RELOAD,
        // | INTERNET_FLAG_NO_AUTO_REDIRECT,
        (DWORD_PTR)this);

    if (request == NULL) {
        reportError("HttpOpenRequestA failed");
        goto out;
    }

    if (!HttpSendRequestA(request, header, 0, data, 0)) {
        reportError("HttpSendRequestA failed");
        goto out;
    }

    if (!readHeaders(request, &headers)) {
        reportError("readHeaders failed");
        goto out;
    }

    if (!InternetQueryDataAvailable(request, &body_size, 0, 0)) {
        reportError("HttpSendRequestA failed");
        goto out;
    }

    if (!InternetReadFile(request, buf,
                          qMin((DWORD)(sizeof(buf) - 1), body_size),
                          &bytes_read)) {
        reportError("InternetReadFile failed");
        goto out;
    }

    qDebug("read %lu bytes\n\n", bytes_read);
    // buf[100] = 0;
    // qDebug("%s\n", buf);

    code = readStatusCode(request);
    if (code <= 0) {
        reportError("readStatusCode failed");
        goto out;
    }

    *response = WinInetHttpResponse(code, QString::fromUtf8(buf));
    ret = true;

out:
    if (request != nullptr) {
        InternetCloseHandle(request);
    }
    if (session != nullptr) {
        InternetCloseHandle(session);
    }
    if (connection != nullptr) {
        InternetCloseHandle(connection);
    }

    return ret;
}

typedef HRESULT(__stdcall* tIEGetProtectedModeCookie)(
    IN LPCWSTR, IN LPCWSTR, OUT LPWSTR, OUT DWORD*, IN DWORD);

bool WinInetHttpReqest::getCookieProtected(const char* cookie_name,
                                           QString* cookie_value)
{
    // int getProtectedCookie(const wchar_t* url, const wchar_t* name)
    static HMODULE ieframelib = NULL;
    static tIEGetProtectedModeCookie pIEGPMC = NULL;
    wchar_t buf[4096] = {L'\0'};
    DWORD size = (DWORD)(sizeof(buf) / sizeof(wchar_t));

    if (!ieframelib) {
        ieframelib = LoadLibraryA("ieframe.dll");
        if (ieframelib) {
            pIEGPMC = (tIEGetProtectedModeCookie)GetProcAddress(
                ieframelib, "IEGetProtectedModeCookie");
        }
    }

    if (!pIEGPMC) {
        return false;
    }

    QString name = QString::fromUtf8(cookie_name);
    if (pIEGPMC(cookieUrl().toStdWString().c_str(),
                name.toStdWString().c_str(), buf, &size,
                kQueryCookieFlag) != S_OK) {
        reportError("failed to read cookie");
        return false;
    }
    qDebug("cookie %s of %s is %s\n", cookie_name,
           cookieUrl().toUtf8().data(),
           QString::fromStdWString(buf).toUtf8().data());
    *cookie_value = QString::fromWCharArray(buf);
    return true;
}

bool WinInetHttpReqest::getCookiePlain(const char* cookie_name,
                                       QString* cookie_value)
{
    char* buf = NULL;
    DWORD len = 0;
    BOOL success = false;
    char url[4096] = {0};
    snprintf(url, sizeof(url), "%s", cookieUrl().toUtf8().data());

    success =
        InternetGetCookieExA(url, cookie_name, NULL, &len, kQueryCookieFlag, 0);
    if (!success) {
        qWarning("failed to get cookie length of the url: %s, the cookie name is: %s, GLE=%lu\n", url,
                 cookie_name, GetLastError());
        return false;
    }
    buf = (char*)malloc((size_t)(len + 1));
    buf[len] = 0;
    success =
        InternetGetCookieExA(url, cookie_name, buf, &len, kQueryCookieFlag, 0);

    if (!success) {
        qWarning("failed to get cookie value of %s::%s, GLE=%lu\n", url,
                 cookie_name, GetLastError());
        return false;
    }

    qDebug("cookie[%s::%s] = %s\n\n", url, cookie_name, buf);

    *cookie_value = QString::fromUtf8(buf);
    return true;
}

bool WinInetHttpReqest::getCookie(const char* cookie_name,
                                  QString* cookie_value)
{
    QString val;
    // if (getCookieProtected(cookie_name, &val) || getCookiePlain(cookie_name,
    // &val)) {
    if (getCookiePlain(cookie_name, &val)) {
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
        QStringList parts = val.split("=", Qt::SkipEmptyParts);
#else
        QStringList parts = val.split("=", QString::SkipEmptyParts);
#endif
        if (parts.size() != 2) {
            return false;
        }
        *cookie_value = parts[1];
        return true;
    }
    return false;
}

QString WinInetHttpReqest::cookieUrl() const
{
    QUrl url = redirected_url_.isValid() ? redirected_url_ : url_;
    return url.toString();
}
