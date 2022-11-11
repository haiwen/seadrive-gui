#include "file-provider-mgr.h"

#include "account.h"
#include "seadrive-gui.h"

FileProviderManager::FileProviderManager() {
    fileProviderListDomains(&domains_);
}

FileProviderManager::~FileProviderManager() {

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
