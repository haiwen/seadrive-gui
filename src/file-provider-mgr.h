#ifndef SEAFILE_CLIENT_FILE_PROVIDER_MANAGER_H
#define SEAFILE_CLIENT_FILE_PROVIDER_MANAGER_H

#include <QObject>
#include <QString>
#include <QMap>

#include "file-provider/file-provider.h"

class Account;

class FileProviderManager : public QObject {
    Q_OBJECT

public:
    FileProviderManager();
    ~FileProviderManager();

    bool registerDomain(const Account account);
    bool unregisterDomain(const Account account);

    void askUserToEnable();

    bool hasEnabledDomains();

    void disconnect(const Account account);

private:
    QString displayName(const Account account);

    QMap<QString, Domain> domains_;
};

#endif // SEAFILE_CLIENT_FILE_PROVIDER_MANAGER_H
