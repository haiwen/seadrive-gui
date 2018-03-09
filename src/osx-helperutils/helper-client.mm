#include <AvailabilityMacros.h>
#import <MPMessagePack/MPXPCClient.h>
#import <Cocoa/Cocoa.h>
#include <QtGlobal>
#include <QString>
#include <QEventLoop>

#import "helper-client.h"
#import "utils/objc-defines.h"

#if !__has_feature(objc_arc)
#error this file must be built with ARC support
#endif

static MPXPCClient *xpc_client_ = nullptr;

HelperClient::HelperClient() : QObject()
{
}

void HelperClient::xpcConnect()
{
    xpc_client_ = [[MPXPCClient alloc]
        initWithServiceName:@"com.seafile.seadrive.helper"
                 privileged:YES
                readOptions:MPMessagePackReaderOptionsUseOrderedDictionary];
    xpc_client_.retryMaxAttempts = 4;
    xpc_client_.retryDelay = 0.5;
    xpc_client_.timeout = 10.0;
}

bool HelperClient::getVersion(QString *version)
{
    ensureConnected();
    QEventLoop q;
    connect(this, &HelperClient::versionDone, &q, &QEventLoop::quit);
    __block bool ok = false;

    [xpc_client_ sendRequest:@"version"
                      params:nil
                  completion:^(NSError *error, NSDictionary *versions) {
                    if (error) {
                        ok = false;
                        qWarning("error when asking for helper version: %s", NSERROR_TO_CSTR(error));
                    } else {
                        ok = true;
                        *version = QString::fromNSString(versions[@"version"]);
                        qWarning("got helper version: %s", version->toUtf8().data());
                    }
                    emit versionDone();
                  }];

    q.exec();
    return ok;
}

void HelperClient::ensureConnected()
{
    if (!xpc_client_) {
        xpcConnect();
    }
}

bool HelperClient::installKext(bool *finished, bool *ok)
{
    ensureConnected();

    NSString *source = [NSBundle.mainBundle.resourcePath stringByAppendingPathComponent:@"osxfuse.fs"];
    NSString *destination = @"/Library/Filesystems/osxfuse.fs";
    NSString *kextID = @"com.github.osxfuse.filesystems.osxfuse";
    // TODO: Use proper path by checking current system version, using this table:
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.10: symbolic link to 10.9
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.11: directory
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.12: symbolic link to 10.11
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.13: symbolic link to 10.11
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.5:  directory
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.6:  directory
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.7:  symbolic link to 10.6
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.8:  symbolic link to 10.6
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.9:  directory
    NSString *kextPath = @"/Library/Filesystems/osxfuse.fs/Contents/Extensions/"
                         @"10.11/osxfuse.kext";
    NSDictionary *params = @{
        @"source" : source,
        @"destination" : destination,
        @"kextID" : kextID,
        @"kextPath" : kextPath
    };

    [xpc_client_
        sendRequest:@"kextInstall"
             params:@[ params ]
         completion:^(NSError *error, id value) {
           *finished = true;
           if (error) {
               qWarning("error when kextInstall: %s", NSERROR_TO_CSTR(error));
               *ok = false;
           } else {
               qWarning("kextInstall success!");
               *ok = true;
           }
         }];

    return true;
}
