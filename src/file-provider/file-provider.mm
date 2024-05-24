#include "file-provider.h"

#include <QDebug>
#include <QMutex>
#include <QWaitCondition>

#import <FinderSync/FinderSync.h>
#import <FileProvider/FileProvider.h>

void fileProviderAskUserToEnable() {
    [FIFinderSyncController showExtensionManagementInterface];
}

bool fileProviderListDomains(QMap<QString, Domain> *domains) {
    qInfo() << "[File Provider] Listing domains";

    bool success = false;
    QMutex mutex;
    QWaitCondition condition;

    [NSFileProviderManager getDomainsWithCompletionHandler:[&](NSArray<NSFileProviderDomain *> *nsdomains, NSError *error) {
        if (error != nil) {
            qWarning() << "[File Provider] Error listing domains:" << error;
            condition.wakeOne();
            return;
        }

        domains->clear();
        for (NSFileProviderDomain *nsdomain in nsdomains) {
            Domain domain;
            domain.identifier = QString::fromNSString(nsdomain.identifier);
            domain.userEnabled = nsdomain.userEnabled;

            domains->insert(domain.identifier, domain);
        }

        success = true;
        condition.wakeOne();
    }];

    mutex.lock();
    condition.wait(&mutex);
    mutex.unlock();

    return success;
}

bool fileProviderAddDomain(const QString domain_id, const QString display_name, bool hidden) {
    qInfo() << "[File Provider] Adding domain" << domain_id << display_name;

    bool success = false;
    QMutex mutex;
    QWaitCondition condition;

    NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:domain_id.toNSString() displayName:display_name.toNSString()];
    domain.hidden = hidden;
    [NSFileProviderManager addDomain:domain completionHandler:[&](NSError *error) {
        if (error != nil) {
            qWarning() << "[File Provider] Error adding domain:" << error;
            condition.wakeOne();
            return;
        }

        success = true;
        condition.wakeOne();
    }];

    mutex.lock();
    condition.wait(&mutex);
    mutex.unlock();

    return success;
}

bool fileProviderRemoveDomain(const QString domain_id, const QString display_name) {
    qInfo() << "[File Provider] Removing domain" << domain_id << display_name;

    bool success = false;
    QMutex mutex;
    QWaitCondition condition;

    NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:domain_id.toNSString() displayName:display_name.toNSString()];
    [NSFileProviderManager removeDomain:domain completionHandler:[&](NSError *error) {
        if (error != nil) {
            qWarning() << "[File Provider] Error removing domain:" << error;
            condition.wakeOne();
            return;
        }

        success = true;
        condition.wakeOne();
    }];

    mutex.lock();
    condition.wait(&mutex);
    mutex.unlock();

    return success;
}

void fileProviderReenumerate(const QString domain_id, const QString display_name) {
    qInfo() << "[File Provider] Reenumerating items";

    QMutex mutex;
    QWaitCondition condition;

    NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:domain_id.toNSString() displayName:display_name.toNSString()];
    NSFileProviderManager *mgr = [NSFileProviderManager managerForDomain:domain];

    [mgr signalEnumeratorForContainerItemIdentifier: NSFileProviderWorkingSetContainerItemIdentifier completionHandler:[&](NSError *error) {
        if (error != nil) {
            qWarning() << "[File Provider] Error reenumerating items:" << error;
            condition.wakeOne();
            return;
        }

        condition.wakeOne();
    }];

    mutex.lock();
    condition.wait(&mutex);
    mutex.unlock();
}

void fileProviderDisconnect(const QString domain_id, const QString display_name) {
    qInfo() << "[File Provider] Disconnect domain";

    QMutex mutex;
    QWaitCondition condition;

    NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:domain_id.toNSString() displayName:display_name.toNSString()];
    NSFileProviderManager *mgr = [NSFileProviderManager managerForDomain:domain];

    [mgr disconnectWithReason:@"Upgrading SeaDrive" options:NSFileProviderManagerDisconnectionOptionsTemporary completionHandler:[&](NSError *error) {
        if (error != nil) {
            qWarning() << "[File Provider] Error disconnect:" << error;
        }
        condition.wakeOne();
        return;
    }];

    mutex.lock();
    condition.wait(&mutex);
    mutex.unlock();
}
