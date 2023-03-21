#include "file-provider-mgr.h"

#include "account.h"
#include "seadrive-gui.h"
#include "utils/utils.h"

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

void FileProviderManager::askUserToEnable() {
    if (!fileProviderListDomains(&domains_)) {
        return;
    }

    if (domains_.isEmpty()) {
        return;
    }

    QMapIterator<QString, Domain> it(domains_);
    while (it.hasNext()) {
        it.next();
        auto domain = it.value();

        if (domain.userEnabled) {
            return;
        }
    }

    gui->messageBox(tr("%1 will ask permissions to enable Finder plugin.").arg(getBrand()));

    fileProviderAskUserToEnable();
}

bool FileProviderManager::hasInvalidDomain() {
    if (!fileProviderListDomains(&domains_)) {
        return false;
    }

    if (domains_.isEmpty()) {
        return false;
    }

    bool found = false;
    QMapIterator<QString, Domain> it(domains_);
    while (it.hasNext()) {
        it.next();
        auto domain = it.value();

        if (domain.userEnabled) {
            found = true;
            break;
        }
    }

    return found;
}

QString FileProviderManager::displayName(const Account account) {
    QString name = account.accountInfo.name;
    if (name.isEmpty()) {
        name = account.username;
    }
    return name + "(" + account.serverUrl.host() + ")";
}
