#include "account.h"
#include "utils/utils.h"
#include "api/requests.h"

QUrl Account::getAbsoluteUrl(const QString& relativeUrl) const
{
    return ::urlJoin(serverUrl, relativeUrl);
}

QString Account::getSignature() const
{
    if (!isValid()) {
        return "";
    }

    return ::md5(serverUrl.host() + username).left(7);
}

QString Account::domainID() const {
#ifdef Q_OS_MAC
    if (!isValid()) {
        return "";
    }

    return ::md5(serverUrl.url() + username);
#else
    return EMPTY_DOMAIN_ID;
#endif
}
