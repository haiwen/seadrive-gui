#include "file-provider.h"

#include <QDebug>
#import <FileProvider/FileProvider.h>

void fileProviderAddDomain(const char *domain_id, const char *display_name, bool hidden) {
    NSString *id = [NSString stringWithUTF8String:domain_id];
    NSString *name = [NSString stringWithUTF8String:display_name];
    NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:id displayName:name];
    domain.hidden = hidden;

    qWarning() << "[File Provider] add domain: id =" << domain_id << ", name =" << display_name;
    [NSFileProviderManager addDomain:domain completionHandler:^(NSError *error) {
        if (error != nil) {
            qWarning() << "[File Provider] failed to add domain:" << error;
        } else {
            qWarning() << "[File Provider] succeed to add domain";
        }
    }];
}

void fileProviderRemoveDomain(const char *domain_id) {
    NSString *id = [NSString stringWithUTF8String:domain_id];
    NSFileProviderDomain *domain = [[NSFileProviderDomain alloc] initWithIdentifier:id displayName:@""];

    qWarning() << "[File Provider] remove domain: id =" << domain_id;
    [NSFileProviderManager removeDomain:domain completionHandler:^(NSError *error) {
        if (error != nil) {
            qWarning() << "[File Provider] failed to remove domain:" << error;
        } else {
            qWarning() << "[File Provider] succeed to remove domain";
        }
    }];
}
