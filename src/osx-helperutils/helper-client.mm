#include <AvailabilityMacros.h>
#import <Cocoa/Cocoa.h>
#import <IOKit/kext/KextManager.h>
#import <MPMessagePack/MPXPCClient.h>
#include <QEventLoop>
#include <QString>
#include <QtGlobal>
#include <QProcess>

#include <grp.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <uuid/uuid.h>

#import "helper-client.h"
#import "utils/objc-defines.h"

#if !__has_feature(objc_arc)
#error this file must be built with ARC support
#endif

#define KEXT_LOCATION @"/Library/Filesystems/osxfuse.fs"
#define KEXT_ID @"com.github.osxfuse.filesystems.osxfuse"

#define MACOSX_ADMIN_GROUP_NAME "admin"
#define OSXFUSE_SYSCTL_TUNABLES_ADMIN "vfs.generic.osxfuse.tunables.admin_group"

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

    // Use an eventloop to translate an async xpc call to a sync call.
    // See
    // https://doc.qt.io/archives/qq/qq27-responsive-guis.html#waitinginalocaleventloop
    QEventLoop q;
    connect(this, &HelperClient::versionDone, &q, &QEventLoop::quit);
    __block bool ok = false;

    [xpc_client_
        sendRequest:@"version"
             params:nil
         completion:^(NSError *error, NSDictionary *versions) {
           if (error) {
               ok = false;
               qWarning("error when asking for helper version: %s",
                        NSERROR_TO_CSTR(error));
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

static bool hasFuseMounts() {
    QProcess mount;
    mount.start("mount");
    mount.waitForFinished();
    QString info(mount.readAllStandardOutput());
    return info.contains("@osxufse");
};

bool HelperClient::needInstallKext()
{
    if (![NSFileManager.defaultManager fileExistsAtPath:KEXT_LOCATION
                                            isDirectory:nil]) {
        return true;
    }

    // Skip install/upgrade of kext if there is already fuse mounts
    if (hasFuseMounts()) {
        return false;
    }

    NSArray *array = @[KEXT_ID];
    NSDictionary *infos = (__bridge NSDictionary*)(KextManagerCopyLoadedKextInfo((__bridge CFArrayRef)array, nil));
    if (![infos count]) {
        return true;
    }

    struct group *admin_group = getgrnam(MACOSX_ADMIN_GROUP_NAME);
    if (admin_group) {
        int current_set_gid;
        size_t len = sizeof(current_set_gid);
        int admin_gid = admin_group->gr_gid;
        if (sysctlbyname(OSXFUSE_SYSCTL_TUNABLES_ADMIN, &current_set_gid, &len, NULL, 0) != 0 || current_set_gid != admin_gid) {
            qWarning("need to reinstall the kext because osxfuse admin_group not set yet");
            return true;
        }
    }

    // TODO: compare version of current installed kext with latest kext, and
    // upgrade if current one is outdated.

    return false;
}

bool HelperClient::installKext(bool *require_user_approval)
{
    if (!needInstallKext()) {
        qWarning("No need to reinstall the kext");
        return true;
    }

    ensureConnected();

    NSString *source = [NSBundle.mainBundle.resourcePath
        stringByAppendingPathComponent:@"osxfuse.fs"];
    NSString *destination = @"/Library/Filesystems/osxfuse.fs";
    // TODO: Use proper path by checking current system version, using this
    // table:
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.10: symbolic link
    // to 10.9
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.11: directory
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.12: symbolic link
    // to 10.11
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.13: symbolic link
    // to 10.11
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.5:  directory
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.6:  directory
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.7:  symbolic link
    // to 10.6
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.8:  symbolic link
    // to 10.6
    // /Library/Filesystems/osxfuse.fs/Contents/Extensions/10.9:  directory
    NSString *kextPath = @"/Library/Filesystems/osxfuse.fs/Contents/Extensions/"
                         @"10.11/osxfuse.kext";
    NSDictionary *params = @{
        @"source" : source,
        @"destination" : destination,
        @"kextID" : KEXT_ID,
        @"kextPath" : kextPath
    };

    QEventLoop q;
    connect(this, &HelperClient::kextInstallDone, &q, &QEventLoop::quit);
    __block bool ok = false;

    [xpc_client_
        sendRequest:@"kextInstall"
             params:@[ params ]
         completion:^(NSError *error, id value) {
           if (error) {
               QString msg(NSERROR_TO_CSTR(error));
               // If kOSKextReturnSystemPolicy (603946981) is in the
               // error message, it means the "secure kernel
               // extensions loading" is blocking us from loading the
               // kext.
               *require_user_approval = msg.contains("603946981");
               qWarning("error when kextInstall: %s", NSERROR_TO_CSTR(error));
               ok = false;
           } else {
               qWarning("kextInstall success!");
               ok = true;
           }
           emit kextInstallDone();
         }];

    q.exec();

    return ok;
}
