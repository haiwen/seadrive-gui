#include "file-provider.h"

#include <QDebug>
#import <FileProvider/FileProvider.h>

bool fileProviderGetDomains(QStringList *list) {
    __block int result = 0;
    NSLock *lock = [[NSLock alloc] init];
    qInfo() << "[File Provider] get domains";
    [NSFileProviderManager getDomainsWithCompletionHandler:
        ^(NSArray<NSFileProviderDomain *> *domains, NSError *error) {
            if (error != nil) {
                qWarning() << "[File Provider] failed to get domains:" << error;
                [lock lock];
                result = 2;
                [lock unlock];
                return;
            }

            qInfo() << "[File Provider] succeed to get domains";
            for (int i = 0; i < [domains count]; i++) {
                NSFileProviderDomain *domain = [domains objectAtIndex:i];
                list->append([domain.identifier UTF8String]);
            }
            [lock lock];
            result = 1;
            [lock unlock];
        }
    ];

    int r = 0;
    while (true) {
        [NSThread sleepForTimeInterval: 1];

        [lock lock];
        r = result;
        [lock unlock];
        if (r) {
            break;
        }
    }

    return r == 1;
}

bool fileProviderAddDomain(const char *domain_id, const char *display_name, bool hidden) {
    NSString *id = [NSString stringWithUTF8String:domain_id];
    NSString *name = [NSString stringWithUTF8String:display_name];
    NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:id displayName:name];
    domain.hidden = hidden;

    __block int result = 0;
    NSLock *lock = [[NSLock alloc] init];
    qInfo() << "[File Provider] add domain: id =" << domain_id << ", name =" << display_name;
    [NSFileProviderManager addDomain:domain completionHandler:^(NSError *error) {
        [lock lock];
        if (error != nil) {
            qWarning() << "[File Provider] failed to add domain:" << error;
            result = 2;
        } else {
            qInfo() << "[File Provider] succeed to add domain";
            result = 1;
        }
        [lock unlock];
    }];

    int r = 0;
    while (true) {
        [NSThread sleepForTimeInterval: 1];

        [lock lock];
        r = result;
        [lock unlock];
        if (r) {
            break;
        }
    }

    return r == 1;
}

bool fileProviderRemoveDomain(const char *domain_id) {
    NSString *id = [NSString stringWithUTF8String:domain_id];
    NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:id displayName:@""];

    __block int result = 0;
    NSLock *lock = [[NSLock alloc] init];
    qInfo() << "[File Provider] remove domain: id =" << domain_id;
    [NSFileProviderManager removeDomain:domain completionHandler:^(NSError *error) {
        [lock lock];
        if (error != nil) {
            qWarning() << "[File Provider] failed to remove domain:" << error;
            result = 2;
        } else {
            qInfo() << "[File Provider] succeed to remove domain";
            result = 1;
        }
        [lock unlock];
    }];

    int r = 0;
    while (true) {
        [NSThread sleepForTimeInterval: 1];

        [lock lock];
        r = result;
        [lock unlock];
        if (r) {
            break;
        }
    }

    return r == 1;
}
