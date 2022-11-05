#include "file-provider-mgr.h"

#include <QTimer>

#include "account.h"
#include "seadrive-gui.h"

namespace {
    const char *kDummyDomainID = "seadrive-dummy-id";
    const char *kDummyDomainName = "seadrive-dummy-name";
}

FileProviderManager::FileProviderManager() {

}

FileProviderManager::~FileProviderManager() {

}

void FileProviderManager::start() {
    fileProviderListDomains(&domains_);

    if (!domains_.contains(kDummyDomainID)) {
        addDummyDomain(true);
        fileProviderListDomains(&domains_);
    }

    if (!domains_[kDummyDomainID].userEnabled) {
        // fileProviderAskUserToEnable() function requires at least one
        // non-hidden domain, so we set the dummy domain to be non-hidden, then
        // set it back again.
        addDummyDomain(false);
        fileProviderAskUserToEnable();
        QTimer::singleShot(1000, this, [&]() {
            addDummyDomain(true);
        });
    }
}

bool FileProviderManager::registerDomain(const Account account) {
    QString id = account.domainID();
    QString name = displayName(account);

    if (domains_.contains(id)) {
        return true;
    }

    if (!fileProviderAddDomain(id, name)) {
        return false;
    }

    fileProviderListDomains(&domains_);
    return true;
}

bool FileProviderManager::unregisterDomain(const Account account) {
    QString id = account.domainID();
    QString name = displayName(account);

    if (!fileProviderRemoveDomain(id, name)) {
        return false;
    }

    fileProviderListDomains(&domains_);
    return true;
}

QString FileProviderManager::displayName(const Account account) {
    QString name = account.accountInfo.name;
    if (name.isEmpty()) {
        name = account.username;
    }
    return name + "(" + account.serverUrl.host() + ")";
}

void FileProviderManager::addDummyDomain(bool hidden) {
    fileProviderAddDomain(kDummyDomainID, kDummyDomainName, hidden);
    fileProviderListDomains(&domains_);
}

void FileProviderManager::removeDummyDomain() {
    fileProviderRemoveDomain(kDummyDomainID, kDummyDomainName);
    fileProviderListDomains(&domains_);
}
