#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QUrl>
#include <QString>
#include <QMetaType>
#include <QDebug>

#include "api/server-info.h"

class AccountInfo {
public:
    QString email;
    QString name;
    qint64 totalStorage;
    qint64 usedStorage;
};

class Account {
    friend class AccountManager;
public:
    ServerInfo serverInfo;
    AccountInfo accountInfo;
    QUrl serverUrl;
    QString username;
    QString token;
#if defined(Q_OS_WIN32)
    QString syncRoot;
    QString syncFolderName;
#endif
    qint64 lastVisited;
    bool isShibboleth;
    bool isAutomaticLogin;
    bool isKerberos;

    Account() : serverInfo(),
                lastVisited(0),
                isShibboleth(false),
                isAutomaticLogin(false),
                isKerberos(false) {}
    Account(QUrl serverUrl, QString username, QString token,
            qint64 lastVisited=0, bool isShibboleth = false,
            bool isAutomaticLogin = true, bool isKerberos = false)
        : serverInfo(),
          accountInfo(),
          serverUrl(serverUrl),
          username(username),
          token(token),
          lastVisited(lastVisited),
          isShibboleth(isShibboleth),
          isAutomaticLogin(isAutomaticLogin),
          isKerberos(isKerberos) {}

    Account(const Account &rhs)
      : serverInfo(rhs.serverInfo),
        accountInfo(rhs.accountInfo),
        serverUrl(rhs.serverUrl),
        username(rhs.username),
        token(rhs.token),
#if defined(Q_OS_WIN32)
        syncRoot(rhs.syncRoot),
        syncFolderName(rhs.syncFolderName),
#endif
        lastVisited(rhs.lastVisited),
        isShibboleth(rhs.isShibboleth),
        isAutomaticLogin(rhs.isAutomaticLogin),
        isKerberos(rhs.isKerberos)
    {
    }

    Account& operator=(const Account&rhs) {
        serverInfo = rhs.serverInfo;
        accountInfo = rhs.accountInfo;
        serverUrl = rhs.serverUrl;
        username = rhs.username;
        token = rhs.token;
#if defined(Q_OS_WIN32)
        syncRoot = rhs.syncRoot;
        syncFolderName = rhs.syncFolderName;
#endif
        lastVisited = rhs.lastVisited;
        isShibboleth = rhs.isShibboleth;
        isAutomaticLogin = rhs.isAutomaticLogin;
        isKerberos = rhs.isKerberos;
        return *this;
    }

    bool operator==(const Account& rhs) const {
        return serverUrl == rhs.serverUrl
            && username == rhs.username;
    }

    bool operator!=(const Account& rhs) const {
        return !(*this == rhs);
    }

    bool isValid() const {
        return token.length() > 0;
    }

    bool isPro() const {
        return serverInfo.proEdition;
    }

    bool hasOfficePreview() const {
        return serverInfo.officePreview;
    }

    bool hasFileSearch() const {
        return serverInfo.fileSearch;
    }

    bool hasDisableSyncWithAnyFolder() const {
        return serverInfo.disableSyncWithAnyFolder;
    }

    bool isAtLeastVersion(unsigned majorVersion, unsigned minorVersion, unsigned patchVersion) const {
        return (serverInfo.majorVersion << 20) +
               (serverInfo.minorVersion << 10) +
               (serverInfo.patchVersion) >=
               (majorVersion << 20) + (minorVersion << 10) + (patchVersion);
    }

    // require pro edtions and version at least at ...
    // excluding OSS Version
    bool isAtLeastProVersion(unsigned majorVersion, unsigned minorVersion, unsigned patchVersion) const {
        return isPro() && isAtLeastVersion(majorVersion, minorVersion, patchVersion);
    }

    // require oss edtions and version at least at ...
    // excluding Pro Version
    bool isAtLeastOSSVersion(unsigned majorVersion, unsigned minorVersion, unsigned patchVersion) const {
        return !isPro() && isAtLeastVersion(majorVersion, minorVersion, patchVersion);
    }

    qint32 getTotalStorage() const {
        return accountInfo.totalStorage;
    }

    qint32 getUsedStorage() const {
        return accountInfo.usedStorage;
    }

    QUrl getAbsoluteUrl(const QString& relativeUrl) const;
    QString getSignature() const;

    QString toString() const
    {
        if (!isValid()) {
            return "<invalid account>";
        }
        return QString("%1 %2 %3")
            .arg(serverUrl.toString())
            .arg(username)
            .arg(token.mid(0, 7));
    }

    QString domainID() const;
};

// Add converter so we can do things like:
//  qDebug() << "account is" << account;
inline QDebug operator<<(QDebug debug, const Account &account)
{
    QDebugStateSaver saver(debug);
    debug.nospace() << account.toString().toUtf8().data();

    return debug;
}



Q_DECLARE_METATYPE(Account)

#endif // ACCOUNT_H
