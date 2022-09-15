#include "file-provider.h"

#include <QDebug>
#import <FileProvider/FileProvider.h>

bool fileProviderAddDomain(const char *domain_id, const char *display_name, bool hidden) {
    NSString *id = [NSString stringWithUTF8String:domain_id];
    NSString *name = [NSString stringWithUTF8String:display_name];
    NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:id displayName:name];
    domain.hidden = hidden;

    __block int result = 0;
    NSLock *lock = [[NSLock alloc] init];
    qWarning() << "[File Provider] add domain: id =" << domain_id << ", name =" << display_name;
    [NSFileProviderManager addDomain:domain completionHandler:^(NSError *error) {
        [lock lock];
        if (error != nil) {
            qWarning() << "[File Provider] failed to add domain:" << error;
            result = 2;
        } else {
            qWarning() << "[File Provider] succeed to add domain";
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
    qWarning() << "[File Provider] remove domain: id =" << domain_id;
    [NSFileProviderManager removeDomain:domain completionHandler:^(NSError *error) {
        [lock lock];
        if (error != nil) {
            qWarning() << "[File Provider] failed to remove domain:" << error;
            result = 2;
        } else {
            qWarning() << "[File Provider] succeed to remove domain";
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
