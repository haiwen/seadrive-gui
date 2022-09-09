#include "file-provider-mgr.h"

#include <QDir>
#include <QThread>
#include <QSettings>
#include <QStringList>

#include "account.h"
#include "file-provider/file-provider.h"

namespace {
    const char *settingsGroup = "file-provider";

    const char *dummyDomainID = "dummy";
    const char *dummyDomainName = "seadrive";
}

FileProviderManager::FileProviderManager() {

}

FileProviderManager::~FileProviderManager() {

}

QString FileProviderManager::workingDir() {
    return QDir::home().absoluteFilePath("Library/Containers/com.seafileltd.seadrive.fileprovider/Data/Documents");
}

void FileProviderManager::start() {
#if defined(Q_OS_MAC)
    addDummyDomain();
#endif
}

void FileProviderManager::registerDomain(const Account account) {
#if defined(Q_OS_MAC)
    if (!account.isValid()) {
        return;
    }

    QString id = account.domainID();
    QString name = displayName(account);
    fileProviderAddDomain(id.toUtf8().constData(), name.toUtf8().constData(), false);

    QThread::sleep(2);
#endif
}

QString FileProviderManager::displayName(const Account account) {
    return account.username;
}

void FileProviderManager::addDummyDomain() {
    QString id(dummyDomainID), name(dummyDomainName);
    fileProviderAddDomain(id.toUtf8().constData(), name.toUtf8().constData(), true);

    QThread::sleep(6);
}

void FileProviderManager::removeDummyDomain() {
    QString id(dummyDomainID);
    fileProviderRemoveDomain(id.toUtf8().constData());
}
