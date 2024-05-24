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

    // If the domain hasn't been registered, fileProviderAddDomain() will register the domain and start the file provider extension (seadrive daemon). Otherwise, calling fileProviderAddDomain() would do nothing.
    // To start the extension for registered domains, we need to call fileProviderReenumerate().
    if (domains_.contains(id)) {
        fileProviderReenumerate(id, name);
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

bool FileProviderManager::hasEnabledDomains() {
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

void FileProviderManager::disconnect(const Account account) {
    QString id = account.domainID();
    QString name = displayName(account);
    fileProviderDisconnect(id, name);
}

void FileProviderManager::connect(const Account account) {
    QString id = account.domainID();
    QString name = displayName(account);
    fileProviderConnect (id, name);
}
