#include "file-provider.h"

#include <QDebug>
#include <QSemaphore>

#import <FinderSync/FinderSync.h>
#import <FileProvider/FileProvider.h>

void fileProviderAskUserToEnable() {
    [FIFinderSyncController showExtensionManagementInterface];
}

bool fileProviderListDomains(QMap<QString, Domain> *domains) {
    qInfo() << "[File Provider] Listing domains";

    bool success = false;
    QSemaphore completed;

    [NSFileProviderManager getDomainsWithCompletionHandler:[&](NSArray<NSFileProviderDomain *> *nsdomains, NSError *error) {
        if (error != nil) {
            qWarning() << "[File Provider] Error listing domains:" << error;
        } else {
            domains->clear();
            for (NSFileProviderDomain *nsdomain in nsdomains) {
                Domain domain;
                domain.identifier = QString::fromNSString(nsdomain.identifier);
                domain.userEnabled = nsdomain.userEnabled;

                domains->insert(domain.identifier, domain);
            }
            success = true;
        }
        completed.release();
    }];

    completed.acquire();

    return success;
}

bool fileProviderAddDomain(const QString domain_id, const QString display_name, bool hidden) {
    qInfo() << "[File Provider] Adding domain" << domain_id << display_name;

    bool success = false;
    QSemaphore completed;

    NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:domain_id.toNSString() displayName:display_name.toNSString()];
    domain.hidden = hidden;
    // When supportsSyncingTrash = NO, files will not be moved to the trash under CloudStorage (this is not the system trash).
    // Some file deletion APIs may trigger behavior that moves files into the CloudStorage trash, and we do not support that behavior.
    // Setting supportsSyncingTrash = NO ensures that when a file is deleted, it is either removed directly or moved to the system trash.
    // In either case, SeaDrive will receive a delete event and can correctly delete the file from the cloud.
    domain.supportsSyncingTrash = NO;
    [NSFileProviderManager addDomain:domain completionHandler:[&](NSError *error) {
        if (error != nil) {
            qWarning() << "[File Provider] Error adding domain:" << error;
        } else {
            success = true;
        }
        completed.release();
    }];

    completed.acquire();

    return success;
}

bool fileProviderRemoveDomain(const QString domain_id, const QString display_name) {
    qInfo() << "[File Provider] Removing domain" << domain_id << display_name;

    bool success = false;
    QSemaphore completed;

    NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:domain_id.toNSString() displayName:display_name.toNSString()];
    [NSFileProviderManager removeDomain:domain completionHandler:[&](NSError *error) {
        if (error != nil) {
            qWarning() << "[File Provider] Error removing domain:" << error;
        } else {
            success = true;
        }
        completed.release();
    }];

    completed.acquire();

    return success;
}

void fileProviderReenumerate(const QString domain_id, const QString display_name) {
    qInfo() << "[File Provider] Reenumerating items";

    QSemaphore completed;

    NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:domain_id.toNSString() displayName:display_name.toNSString()];
    NSFileProviderManager *mgr = [NSFileProviderManager managerForDomain:domain];

    if (mgr == nil) {
        qWarning() << "[File Provider] Cannot reenumerate items because the manager is unavailable for domain" << domain_id;
        return;
    }

    [mgr signalEnumeratorForContainerItemIdentifier: NSFileProviderWorkingSetContainerItemIdentifier completionHandler:[&](NSError *error) {
        if (error != nil) {
            qWarning() << "[File Provider] Error reenumerating items:" << error;
        }
        completed.release();
    }];

    completed.acquire();
}

void fileProviderDisconnect(const QString domain_id, const QString display_name) {
    qInfo() << "[File Provider] Disconnect domain";

    QSemaphore completed;

    NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:domain_id.toNSString() displayName:display_name.toNSString()];
    NSFileProviderManager *mgr = [NSFileProviderManager managerForDomain:domain];

    if (mgr == nil) {
        qWarning() << "[File Provider] Cannot disconnect because the manager is unavailable for domain" << domain_id;
        return;
    }

    [mgr disconnectWithReason:@"Upgrading SeaDrive" options:NSFileProviderManagerDisconnectionOptionsTemporary completionHandler:[&](NSError *error) {
        if (error != nil) {
            qWarning() << "[File Provider] Error disconnecting domain:" << error;
        }
        completed.release();
    }];

    completed.acquire();
}

void fileProviderConnect (const QString domain_id, const QString display_name) {
    qInfo() << "[File Provider] Connect domain";

    QSemaphore completed;

    NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:domain_id.toNSString() displayName:display_name.toNSString()];
    NSFileProviderManager *mgr = [NSFileProviderManager managerForDomain:domain];

    if (mgr == nil) {
        qWarning() << "[File Provider] Cannot reconnect because the manager is unavailable for domain" << domain_id;
        return;
    }

    [mgr reconnectWithCompletionHandler:[&](NSError *error) {
        if (error != nil) {
            qWarning() << "[File Provider] Error connect domain:" << error;
        }
        completed.release();
    }];

    completed.acquire();
}

bool fileProviderGetUserVisibleURL(const QString domain_id, const QString display_name, QUrl *url)
{
    qInfo() << "[File Provider] Placeholder URL";

    bool success = false;
    QSemaphore completed;

    NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:domain_id.toNSString() displayName:display_name.toNSString()];
    NSFileProviderManager *mgr = [NSFileProviderManager managerForDomain:domain];

    if (mgr == nil) {
        qWarning() << "[File Provider] Cannot get the user-visible root URL because the manager is unavailable for domain" << domain_id;
        return false;
    }

    [mgr getUserVisibleURLForItemIdentifier:NSFileProviderRootContainerItemIdentifier completionHandler:[&](NSURL *userVisibleFile, NSError *error) {
        if (error != nil) {
            qWarning() << "[File Provider] Failed to get the user-visible root URL for domain" << domain_id << ":" << error;
        } else if (userVisibleFile == nil) {
            qWarning() << "[File Provider] No user-visible root URL was returned for domain" << domain_id;
        } else {
            *url = QUrl::fromNSURL(userVisibleFile);
            success = true;
        }
        completed.release();
    }];

    // Preserve completion even if the callback runs before we start waiting.
    completed.acquire();

    return success;
}
