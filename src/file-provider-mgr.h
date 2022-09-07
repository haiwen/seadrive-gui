#ifndef SEAFILE_CLIENT_FILE_PROVIDER_MANAGER_H
#define SEAFILE_CLIENT_FILE_PROVIDER_MANAGER_H

#include <QObject>
#include <QString>

class Account;

class FileProviderManager : public QObject {
    Q_OBJECT

public:
    FileProviderManager();
    ~FileProviderManager();

    QString workingDir();
    void start();

    void registerDomain(const Account account);

private:
    QString displayName(const Account account);

    void addDummyDomain();
    void removeDummyDomain();
};

#endif // SEAFILE_CLIENT_FILE_PROVIDER_MANAGER_H
