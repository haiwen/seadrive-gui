#ifndef SEAFILE_CLIENT_FILE_PROVIDER_MANAGER_H
#define SEAFILE_CLIENT_FILE_PROVIDER_MANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

class Account;

class FileProviderManager : public QObject {
    Q_OBJECT

public:
    FileProviderManager();
    ~FileProviderManager();

    void start();

    bool registerDomain(const Account account);
    bool unregisterDomain(const Account account);

private:
    QString displayName(const Account account);

    void addDummyDomain();
    void removeDummyDomain();

    QStringList domain_ids_;
};

#endif // SEAFILE_CLIENT_FILE_PROVIDER_MANAGER_H
