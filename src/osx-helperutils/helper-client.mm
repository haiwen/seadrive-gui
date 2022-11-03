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
#import "utils/file-utils.h"
#import "utils/utils-mac.h"

#if !__has_feature(objc_arc)
#error this file must be built with ARC support
#endif

#define KEXT_LOCATION @"/Library/Filesystems/macfuse.fs"
#define KEXT_ID @"io.macfuse.filesystems.macfuse"

#define MACOSX_ADMIN_GROUP_NAME "admin"
#define MACFUSE_SYSCTL_TUNABLES_ADMIN "vfs.generic.macfuse.tunables.admin_group"

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


// ret is
//  1  if s1 > s2
//  0  if s1 = s2
//  -1 if s1 < s2
static int compareVersions(const QString &s1, const QString &s2, int *ret)
{
    QStringList v1 = s1.split(".");
    QStringList v2 = s2.split(".");

    int i = 0;
    while (i < v1.size() && i < v2.size()) {
        bool ok;
        int a = v1[i].toInt(&ok);
        if (!ok) {
            return -1;
        }
        int b = v2[i].toInt(&ok);
        if (!ok) {
            return -1;
        }

        if (a > b) {
            *ret = 1;
            return 0;
        } else if (a < b) {
            *ret = -1;
            return 0;
        }

        i++;
    }

    *ret = v1.size() - v2.size();

    return 0;
}

class VersionString
{
public:
    VersionString(const QString &_v) : v(_v)
    {
    }
    bool operator<(const VersionString &rhs) const
    {
        int ret;
        if (compareVersions(v, rhs.v, &ret) < 0) {
            return false;
        }

        return ret < 0;
    }
    const QString v;
};

static QString readValueFromVersionFile(const QString &plist_path, NSString *key)
{
    NSDictionary *infos =
        [NSDictionary dictionaryWithContentsOfFile:plist_path.toNSString()];
    return QString::fromNSString(infos[key]);
}

static QString getBundledExtVersion()
{
    QString bundled_ext_dir =
        QString::fromNSString([NSBundle.mainBundle.resourcePath
            stringByAppendingPathComponent:@"macfuse.fs"]);

    QString versions_plist =
        ::pathJoin(bundled_ext_dir, "Contents", "version.plist");

    return readValueFromVersionFile(versions_plist, @"CFBundleVersion");
}

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

    QString installed_kext_version = QString::fromNSString(infos[KEXT_ID][@"CFBundleVersion"]);
    qWarning("installed kernel extension version is %s", installed_kext_version.toUtf8().data());
    QString latest_kext_version = getBundledExtVersion();
    qWarning("latest extension version is %s", latest_kext_version.toUtf8().data());
    if (VersionString(installed_kext_version) < VersionString(latest_kext_version)) {
        qWarning("installed kernel extension is out-dated");
        return true;
    }

    struct group *admin_group = getgrnam(MACOSX_ADMIN_GROUP_NAME);
    if (admin_group) {
        int current_set_gid;
        size_t len = sizeof(current_set_gid);
        int admin_gid = admin_group->gr_gid;
        if (sysctlbyname(MACFUSE_SYSCTL_TUNABLES_ADMIN, &current_set_gid, &len, NULL, 0) != 0 || current_set_gid != admin_gid) {
            qWarning("need to reinstall the kext because macfuse admin_group not set yet");
            return true;
        }
    }

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
        stringByAppendingPathComponent:@"macfuse.fs"];
    NSString *destination = @"/Library/Filesystems/macfuse.fs";
    // TODO: Use proper path by checking current system version, using this
    // table:
    // /Library/Filesystems/macfuse.fs/Contents/Extensions/10.9:   directory
    // /Library/Filesystems/macfuse.fs/Contents/Extensions/10.10:  symbolic link
    // to 10.9
    // /Library/Filesystems/macfuse.fs/Contents/Extensions/10.11:  directory
    // /Library/Filesystems/macfuse.fs/Contents/Extensions/10.12:  directory
    // /Library/Filesystems/macfuse.fs/Contents/Extensions/10.13:  symbolic link
    // to 10.12
    // /Library/Filesystems/macfuse.fs/Contents/Extensions/10.14:  symbolic link
    // to 10.12
    // /Library/Filesystems/macfuse.fs/Contents/Extensions/10.15:  symbolic link
    // to 10.12
    // /Library/Filesystems/macfuse.fs/Contents/Extensions/10.16:  symbolic link
    // to 11
    // /Library/Filesystems/macfuse.fs/Contents/Extensions/11:     directory
    // /Library/Filesystems/macfuse.fs/Contents/Extensions/12:     symbolic link
    // to 11
    // /Library/Filesystems/macfuse.fs/Contents/Extensions/13:     symbolic link
    // to 11
    NSString *kextPath;
    if (utils::mac::isAtLeastSystemVersion(10, 16, 0)) {
        kextPath = @"/Library/Filesystems/macfuse.fs/Contents/Extensions/"
                         @"11/macfuse.kext";
    } else {
        kextPath = @"/Library/Filesystems/macfuse.fs/Contents/Extensions/"
                         @"10.12/macfuse.kext";
    }
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
