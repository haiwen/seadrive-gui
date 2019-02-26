#include <cstdio>
#include <windows.h>
#include <wininet.h>
#include <QString>
#include <QUrlQuery>
#include <qapplication.h>

#include "win-http-request.h"


const char* STG_SSO_SERVER =
    "http://passo-core-iis-stg.app.paic.com.cn/convert/transfer.asp";

typedef enum Method {
    USE_PLAIN = 0,
    USE_EX,
    USE_PROTECT,
} Method;

void try_get_cookie(const char* url, const char* cookie_name, Method method)
{
    char* value = NULL;
    DWORD len = 0;
    BOOL success = false;

    /* first get the length */
    if (method == USE_PLAIN) {
        success = InternetGetCookieA(url, cookie_name, NULL, &len);
    } else if (method == USE_EX) {
        success = InternetGetCookieExA(url, cookie_name, NULL, &len,
                                       INTERNET_COOKIE_HTTPONLY, 0);
    } else if (method == USE_PROTECT) {
        /* success = IEGetProtectedModeCookie(url, cookie_name, NULL, &len, 0); */
        success = FALSE;
    }

    if (!success) {
        printf("failed to get cookie length of %s::%s, GLE=%lu\n", url,
               cookie_name, GetLastError());
        return;
    }
    value = (char *)malloc((size_t)(len + 1));
    if (method == USE_PLAIN) {
        success = InternetGetCookieA(url, cookie_name, value, &len);
    } else if (method == USE_EX) {
        success = InternetGetCookieExA(url, cookie_name, value, &len,
                                       INTERNET_COOKIE_HTTPONLY, 0);
    } else if (method == USE_PROTECT) {
        /* success = IEGetProtectedModeCookie(url, cookie_name, value, &len, 0); */
        success = FALSE;
    }

    if (!success) {
        printf("failed to get cookie value of %s::%s, GLE=%lu\n", url,
               cookie_name, GetLastError());
        return;
    }

    printf("cookie[%s::%s] = %s\n\n", url, cookie_name, value);
}

void myMessageOutput(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug: %s\n", localMsg.constData());
        break;
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s\n", localMsg.constData());
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s\n", localMsg.constData());
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s\n", localMsg.constData());
        abort();
    }
}

// Demo program that covers the work flow of PA SSO.
int main(int argc, char* argv[])
{
    qInstallMessageHandler(myMessageOutput);
    // printf ("program started\n");
    QUrl url(argc > 1 ? argv[1] : STG_SSO_SERVER);
    QString method = argc > 2 ? argv[2] : "GET";
    QString target =
        argc > 3 ? argv[3] : "http://sharefile.paic.com.cn/sso/login";
    QUrlQuery query;
    query.addQueryItem(QUrl::toPercentEncoding("urltarget"),
                       QUrl::toPercentEncoding(target));
    url.setQuery(query);
    // printf("visiting %s\n", url.toString().toUtf8().data());

    WinInetHttpReqest request(url);
    WinInetHttpResponse response;
    if (!request.send(&response)) {
        return -1;
    }
    // printf ("%s\n", response.toUtf8().data());
    char *u = strdup(url.toString(QUrl::RemovePath).toUtf8().data());
    // try_get_cookie(u, "csrftoken", USE_EX);
    // try_get_cookie(u, "csrftoken", USE_PLAIN);
    // try_get_cookie(u, "sessionid", USE_EX);
    // try_get_cookie(u, "sessionid", USE_PLAIN);
    QString value;
    if (request.getCookie("csrftoken", &value)) {
        printf ("csrftoken = %s\n", value.toUtf8().data());
    } else {
        printf ("failed to get csrftoken\n");
    }
    if (request.getCookie("sessionid", &value)) {
        printf ("sessionid = %s\n", value.toUtf8().data());
    } else {
        printf ("failed to get sessionid\n");
    }
    return 0;
}
