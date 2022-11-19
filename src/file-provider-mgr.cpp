#include "file-provider-mgr.h"

#include "account.h"
#include "seadrive-gui.h"

FileProviderManager::FileProviderManager() {

}

FileProviderManager::~FileProviderManager() {

}

bool FileProviderManager::registerDomain(const Account account) {
    QString id = account.domainID();
    QString name = displayName(account);

    fileProviderListDomains(&domains_);
    if (domains_.contains(id)) {
        return true;
    }

    if (!fileProviderAddDomain(id, name)) {
        return false;
    }

    return true;
}

bool FileProviderManager::unregisterDomain(const Account account) {
    QString id = account.domainID();
    QString name = displayName(account);

    if (!fileProviderRemoveDomain(id, name)) {
        return false;
    }

    return true;
}

QString FileProviderManager::displayName(const Account account) {
    QString name = account.accountInfo.name;
    if (name.isEmpty()) {
        name = account.username;
    }
    return name + "(" + account.serverUrl.host() + ")";
}
