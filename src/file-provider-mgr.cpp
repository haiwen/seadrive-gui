#include "file-provider-mgr.h"

#include <QDir>

#include "account.h"
#include "seadrive-gui.h"
#include "file-provider/file-provider.h"

namespace {
    const char *kDummyDomainID = "seadrive-dummy-id";
    const char *kDummyDomainName = "seadrive-dummy-name";
}

FileProviderManager::FileProviderManager() {

}

FileProviderManager::~FileProviderManager() {

}

void FileProviderManager::start() {
    fileProviderGetDomains(&domain_ids_);

    // The seadrive daemon program is loaded by operating system as a plugin on
    // macOS. There must be an associated domain, otherwise the plugin won't be
    // loaded. As we require the seadrive daemon being started before adding
    // the first account, a dummy domain is created here to help.
    addDummyDomain();
}

bool FileProviderManager::registerDomain(const Account account) {
    QString id = account.domainID();
    QString name = displayName(account);

    if (!domain_ids_.contains(id)) {
        return fileProviderAddDomain(id.toUtf8().constData(), name.toUtf8().constData(), false);
    }

    return true;
}

bool FileProviderManager::unregisterDomain(const Account account) {
    QString id = account.domainID();
    QString name = displayName(account);

    return fileProviderRemoveDomain(id.toUtf8().constData());
}

QString FileProviderManager::displayName(const Account account) {
    QString name = account.accountInfo.name;
    if (name.isEmpty()) {
        name = account.username;
    }
    return name + "(" + account.serverUrl.host() + ")";
}

void FileProviderManager::addDummyDomain() {
    QString id(kDummyDomainID), name(kDummyDomainName);

    if (!domain_ids_.contains(id)) {
        fileProviderAddDomain(id.toUtf8().constData(), name.toUtf8().constData(), true);
    }
}

void FileProviderManager::removeDummyDomain() {
    QString id(kDummyDomainID);

    fileProviderRemoveDomain(id.toUtf8().constData());
}
