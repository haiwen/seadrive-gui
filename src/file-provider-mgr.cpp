#include "file-provider-mgr.h"

#include <QDir>
#include <QThread>
#include <QSettings>
#include <QStringList>

#include "account.h"
#include "seadrive-gui.h"
#include "file-provider/file-provider.h"

namespace {
    const char *kDomainGroup = "domains";
    const char *kDomainSettings = "domain.ini";

    const char *kDummyDomainID = "dummy";
    const char *kDummyDomainName = "seadrive";
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

bool FileProviderManager::registerDomain(const Account account) {
#if defined(Q_OS_MAC)
    QString id = account.domainID();
    QString name = displayName(account);
    QString filename = QDir(gui->seadriveDir()).filePath(kDomainSettings);

    QSettings settings(filename, QSettings::IniFormat);
    settings.beginGroup(kDomainGroup);
    if (!settings.value(id, false).toBool()) {
        settings.setValue(id, true);
        settings.sync();

        return fileProviderAddDomain(id.toUtf8().constData(), name.toUtf8().constData(), false);
    }
#endif

    return true;
}

bool FileProviderManager::unregisterDomain(const Account account) {
#if defined(Q_OS_MAC)
    QString id = account.domainID();
    QString name = displayName(account);
    QString filename = QDir(gui->seadriveDir()).filePath(kDomainSettings);

    QSettings settings(filename, QSettings::IniFormat);
    settings.beginGroup(kDomainGroup);
    if (settings.value(id, false).toBool()) {
        settings.setValue(id, false);
        settings.sync();

        return fileProviderRemoveDomain(id.toUtf8().constData());
    }
#endif

    return true;
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
    QString filename = QDir(gui->seadriveDir()).filePath(kDomainSettings);

    QSettings settings(filename, QSettings::IniFormat);
    settings.beginGroup(kDomainGroup);
    if (!settings.value(id, false).toBool()) {
        settings.setValue(id, true);
        settings.sync();

        fileProviderAddDomain(id.toUtf8().constData(), name.toUtf8().constData(), true);
    }
}

void FileProviderManager::removeDummyDomain() {
    QString id(kDummyDomainID);
    fileProviderRemoveDomain(id.toUtf8().constData());
}
