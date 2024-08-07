#include "utils-mac.h"

#include <AvailabilityMacros.h>
#import <Cocoa/Cocoa.h>
#import <Security/Security.h>

#include <openssl/asn1.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#import <QtDebug>
#import <QStandardPaths>
#import <QOperatingSystemVersion>
#import "utils.h"
#import "file-utils.h"

#if !__has_feature(objc_arc)
#error this file must be built with ARC support
#endif

// borrowed from AvailabilityMacros.h
#ifndef MAC_OS_X_VERSION_10_10
#define MAC_OS_X_VERSION_10_10      101000
#endif
#ifndef MAC_OS_X_VERSION_10_9
#define MAC_OS_X_VERSION_10_9         1090
#endif
#ifndef MAC_OS_X_VERSION_10_8
#define MAC_OS_X_VERSION_10_8         1080
#endif
#ifndef MAC_OS_X_VERSION_10_7
#define MAC_OS_X_VERSION_10_7         1070
#endif

// ****************************************************************************
// utils::mac::getSystemVersion
// utils::mac::isAtLeastSystemVersion
// ****************************************************************************

namespace utils {
namespace mac {
namespace {
unsigned osver_major = 0;
unsigned osver_minor = 0;
unsigned osver_patch = 0;
inline bool isInitializedSystemVersion() { return osver_major != 0; }
inline void initializeSystemVersion() {
    if (isInitializedSystemVersion())
        return;

    auto current_os_version = QOperatingSystemVersion::current();
    osver_major = current_os_version.majorVersion();
    osver_minor = current_os_version.minorVersion();
    osver_patch = current_os_version.microVersion();
}

inline bool _isAtLeastSystemVersion(unsigned major, unsigned minor, unsigned patch)
{
    initializeSystemVersion();
#define OSVER_TO_NUM(major, minor, patch) ((major << 20) + (minor << 10) + (patch))
#define OSVER_SYS() OSVER_TO_NUM(osver_major, osver_minor, osver_patch)
    if (OSVER_SYS() < OSVER_TO_NUM(major, minor, patch)) {
        return false;
    }
#undef OSVER_SYS
#undef OSVER_TO_NUM
    return true;
}
// compile statically
template<unsigned major, unsigned minor, unsigned patch>
inline bool isAtLeastSystemVersion()
{
    return _isAtLeastSystemVersion(major, minor, patch);
}
} // anonymous namespace

void getSystemVersion(unsigned *major, unsigned *minor, unsigned *patch) {
    initializeSystemVersion();
    *major = osver_major;
    *minor = osver_minor;
    *patch = osver_patch;
}

bool isAtLeastSystemVersion(unsigned major, unsigned minor, unsigned patch)
{
    return _isAtLeastSystemVersion(major, minor, patch);
}

bool isOSXYosemiteOrGreater()
{
    return isAtLeastSystemVersion<10, 10, 0>();
}

bool isOSXMavericksOrGreater()
{
    return isAtLeastSystemVersion<10, 9, 0>();
}

bool isOSXMountainLionOrGreater()
{
    return isAtLeastSystemVersion<10, 8, 0>();
}

bool isOSXLionOrGreater()
{
    return isAtLeastSystemVersion<10, 7, 0>();
}

} // namespace mac
} // namesapce utils

// ****************************************************************************
// others
// ****************************************************************************
namespace utils {
namespace mac {

// another solution: hide dock icon when mainwindows is closed and show when
// mainwindows is shown
// http://stackoverflow.com/questions/16994331/multiprocessing-qt-app-how-can-i-limit-it-to-a-single-icon-in-the-macos-x-dock
void setDockIconStyle(bool hidden) {
    ProcessSerialNumber psn = { 0, kCurrentProcess };
    OSStatus err;
    if (hidden) {
        // kProcessTransformToBackgroundApplication is not support on OSX 10.7 and before
        // kProcessTransformToUIElementApplication is used for better fit when possible
        unsigned major;
        unsigned minor;
        unsigned patch;
        getSystemVersion(&major, &minor, &patch);
        if (major == 10 && minor == 7)
            err = TransformProcessType(&psn, kProcessTransformToBackgroundApplication);
        else
            err = TransformProcessType(&psn, kProcessTransformToUIElementApplication);
    } else {
        // kProcessTransformToForegroundApplication is supported on OSX 10.6 or later
        err = TransformProcessType(&psn, kProcessTransformToForegroundApplication);
    }
    if (err != noErr)
        qWarning("setDockIconStyle %s failure, status code: %d\n", (hidden ? "hidden" : "show"), err);
}

void orderFrontRegardless(unsigned long long win_id, bool force) {
    NSView __weak *widget =  (__bridge NSView*)(void*)win_id;
    NSWindow *window = [widget window];
    if(force || [window isVisible])
        [window performSelector:@selector(orderFrontRegardless) withObject:nil afterDelay:0.05];
}

// https://bugreports.qt-project.org/browse/QTBUG-40449 is fixed in QT 5.4.1
// TODO remove this and related code once qt 5.4.1 is widely used
QString fix_file_id_url(const QString &path) {
    if (!path.startsWith("/.file/id="))
        return path;
    const QString url = "file://" + path;
    NSString *fileIdURL = [NSString stringWithCString:url.toUtf8().data()
                                    encoding:NSUTF8StringEncoding];
    NSURL *goodURL = [[NSURL URLWithString:fileIdURL] filePathURL];
    NSString *filePath = goodURL.path; // readonly

    QString retval = QString::fromUtf8([filePath UTF8String],
                                       [filePath lengthOfBytesUsingEncoding:NSUTF8StringEncoding]);
    return retval;
}

// original idea come from growl framework
// http://growl.info/about
bool get_auto_start()
{
    NSURL *itemURL = [[NSBundle mainBundle] bundleURL];
    CFURLRef URLToToggle = (__bridge CFURLRef)itemURL;

    bool found = false;

    LSSharedFileListRef loginItems = LSSharedFileListCreate(NULL, kLSSharedFileListSessionLoginItems, NULL);
    if (loginItems) {
        UInt32 seed = 0U;
        CFArrayRef currentLoginItems = LSSharedFileListCopySnapshot(loginItems,
                                                                    &seed);
        const CFIndex count = CFArrayGetCount(currentLoginItems);
        for (CFIndex idx = 0; idx < count; ++idx) {
            LSSharedFileListItemRef item = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(currentLoginItems, idx);
            CFURLRef outURL = NULL;

            const UInt32 resolutionFlags = kLSSharedFileListNoUserInteraction | kLSSharedFileListDoNotMountVolumes;
#if (__MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_10)
            outURL = LSSharedFileListItemCopyResolvedURL(item, resolutionFlags, /*outError*/ NULL);
            if (outURL == NULL) {
                continue;
            }
#else
            OSStatus err = LSSharedFileListItemResolve(item, resolutionFlags, &outURL, /*outRef*/ NULL);
            if (err != noErr || outURL == NULL) {
                if (outURL)
                    CFRelease(outURL);
                continue;
            }
#endif
            found = CFEqual(outURL, URLToToggle);
            CFRelease(outURL);

            if (found)
                break;
        }
        if (currentLoginItems)
            CFRelease(currentLoginItems);
        CFRelease(loginItems);
    }
    return found;
}

void set_auto_start(bool enabled)
{
    NSURL *itemURL = [[NSBundle mainBundle] bundleURL];
    CFURLRef URLToToggle = (__bridge CFURLRef)itemURL;

    LSSharedFileListRef loginItems = LSSharedFileListCreate(kCFAllocatorDefault, kLSSharedFileListSessionLoginItems, /*options*/ NULL);
    if (loginItems) {
        UInt32 seed = 0U;
        Boolean found;
        LSSharedFileListItemRef existingItem = NULL;

        CFArrayRef currentLoginItems = LSSharedFileListCopySnapshot(loginItems,
                                                                    &seed);
        const CFIndex count = CFArrayGetCount(currentLoginItems);
        for (CFIndex idx = 0; idx < count; ++idx) {
            LSSharedFileListItemRef item = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(currentLoginItems, idx);
            CFURLRef outURL = NULL;

            const UInt32 resolutionFlags = kLSSharedFileListNoUserInteraction | kLSSharedFileListDoNotMountVolumes;
#if (__MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_10)
            outURL = LSSharedFileListItemCopyResolvedURL(item, resolutionFlags, /*outError*/ NULL);
            if (outURL == NULL) {
                continue;
            }
#else
            OSStatus err = LSSharedFileListItemResolve(item, resolutionFlags, &outURL, /*outRef*/ NULL);
            if (err != noErr || outURL == NULL) {
                if (outURL)
                    CFRelease(outURL);
                continue;
            }
#endif
            found = CFEqual(outURL, URLToToggle);
            CFRelease(outURL);

            if (found) {
                existingItem = item;
                break;
            }
        }

        if (enabled && !found) {
            NSString *displayName = @"SeaDrive Client";
            IconRef icon = NULL;
            FSRef ref;
            // TODO: replace the deprecated CFURLGetFSRef
            Boolean gotRef = CFURLGetFSRef(URLToToggle, &ref);
            if (gotRef) {
                OSStatus err = GetIconRefFromFileInfo(
                    &ref,
                    /*fileNameLength*/ 0,
                    /*fileName*/ NULL, kFSCatInfoNone,
                    /*catalogInfo*/ NULL, kIconServicesNormalUsageFlag, &icon,
                    /*outLabel*/ NULL);
                if (err != noErr) {
                    if (icon)
                        ReleaseIconRef(icon);
                    icon = NULL;
                }
            }

            LSSharedFileListItemRef newItem = LSSharedFileListInsertItemURL(
                loginItems, kLSSharedFileListItemBeforeFirst,
                (__bridge CFStringRef)displayName, icon, URLToToggle,
                /*propertiesToSet*/ NULL, /*propertiesToClear*/ NULL);
            if (newItem)
                CFRelease(newItem);
            if (icon)
                ReleaseIconRef(icon);
        } else if (!enabled && found) {
            LSSharedFileListItemRemove(loginItems, existingItem);
        }

        if (currentLoginItems)
            CFRelease(currentLoginItems);
        CFRelease(loginItems);
    }
}

void copyTextToPasteboard(const QString &text) {
    NSString *text_data = [NSString stringWithUTF8String:text.toUtf8().data()];
    NSPasteboard *paste_board = [NSPasteboard generalPasteboard];
    [paste_board clearContents];
    [paste_board declareTypes:[NSArray arrayWithObjects:NSStringPboardType, nil] owner:nil];
    [paste_board writeObjects:@[text_data]];
}

QString mainBundlePath() {
    NSURL *url = [[NSBundle mainBundle] bundleURL];
    return [[url path] UTF8String];
}

static inline bool isSslPolicy(SecPolicyRef policy) {
    bool is_ssl = false;
    CFDictionaryRef properties = NULL;
    if (!policy)
        return false;
    if ((properties = SecPolicyCopyProperties(policy)) == NULL)
        return false;
    CFTypeRef value = NULL;
    if (CFDictionaryGetValueIfPresent(properties, kSecPolicyOid,
                                      (const void **)&value) &&
        CFEqual(value, kSecPolicyAppleSSL))
        is_ssl = true;

    ;
    CFRelease(properties);
    return is_ssl;
}

static bool isCertificateDistrustedByUser(SecCertificateRef cert,
                                          SecTrustSettingsDomain domain) {
    CFArrayRef trustSettings;
    // On return, an array of CFDictionary objects specifying the trust settings
    // for the certificate
    OSStatus status = SecTrustSettingsCopyTrustSettings(cert, domain, &trustSettings);
    if (status != errSecSuccess)
        return false;

    bool distrusted = false;

    CFNumberRef result;
    SecTrustSettingsResult result_val;
    CFIndex size = CFArrayGetCount(trustSettings);
    for (CFIndex i = 0; i < size; ++i) {
        CFDictionaryRef trustSetting = (CFDictionaryRef)CFArrayGetValueAtIndex(trustSettings, i);
        SecPolicyRef policy = (SecPolicyRef)CFDictionaryGetValue(trustSetting, kSecTrustSettingsPolicy);

        if (isSslPolicy(policy) &&
            CFDictionaryGetValueIfPresent(trustSetting, kSecTrustSettingsResult,
                                          (const void **)&result)) {
            if (!CFNumberGetValue(result, kCFNumberIntType, &result_val))
                continue;
            switch (result_val) {
            case kSecTrustSettingsResultTrustRoot:
            case kSecTrustSettingsResultTrustAsRoot:
            case kSecTrustSettingsResultUnspecified:
                distrusted = false;
                break;
            case kSecTrustSettingsResultInvalid:
            case kSecTrustSettingsResultDeny:
            default:
                distrusted = true;
                break;
            }

            break;
        }
    }

    CFRelease(trustSettings);

    return distrusted;
}

bool isCertExpired(const unsigned char **bytes, size_t len) {
    using X509_ptr = std::unique_ptr<X509, decltype(&X509_free)>;
    X509_ptr cert(d2i_X509(NULL, bytes, len), X509_free);
    return X509_cmp_current_time (X509_get_notAfter(cert.get())) < 0;
}


static void
appendCaCertificateFromSecurityStore(std::vector<QByteArray> *retval,
                                     SecTrustSettingsDomain domain) {
    CFArrayRef certs;
    OSStatus status = 1;
    status = SecTrustSettingsCopyCertificates(domain, &certs);
    if (status != errSecSuccess)
        return;

    CFIndex size = CFArrayGetCount(certs);
    for (CFIndex i = 0; i < size; ++i) {
        SecCertificateRef cert =
            (SecCertificateRef)CFArrayGetValueAtIndex(certs, i);

        if (isCertificateDistrustedByUser(cert, kSecTrustSettingsDomainSystem) ||
            isCertificateDistrustedByUser(cert, kSecTrustSettingsDomainAdmin) ||
            isCertificateDistrustedByUser(cert, kSecTrustSettingsDomainUser)) {
            CFStringRef name;
            status = SecCertificateCopyCommonName(cert, &name);
            if (status == errSecSuccess && name != nil) {
                qWarning("declining a distrusted CA certificate from the system"
                         "store with common name %s",
                         [(__bridge NSString*)name UTF8String]);
                CFRelease(name);
            }
            else
                qWarning("declining a distrusted CA certificate from the system store");
            continue;
        }

        // copy if trusted
        CFDataRef data;
        data = SecCertificateCopyData(cert);

        if (data == NULL) {
            qWarning("error retrieving a CA certificate from the system store");
        } else {
            QByteArray raw_data((const char *)CFDataGetBytePtr(data), CFDataGetLength(data));
            const unsigned char *pdata = (const unsigned char *)CFDataGetBytePtr(data);
            if (!isCertExpired(&pdata, raw_data.size())) {
                retval->push_back(raw_data);
            }
            CFRelease(data);
        }
    }
    CFRelease(certs);
}

std::vector<QByteArray> getSystemCaCertificates() {
    std::vector<QByteArray> retval;
    appendCaCertificateFromSecurityStore(&retval, kSecTrustSettingsDomainSystem);
    appendCaCertificateFromSecurityStore(&retval, kSecTrustSettingsDomainAdmin);
    appendCaCertificateFromSecurityStore(&retval, kSecTrustSettingsDomainUser);
    return retval;
}

// Create a symlink, and adding the link to the finder favorite list
// instead of the real drive folder, which is a volume and sometimes
// finder can't handle it properly.
bool createLocalSymLink(const QString &path, QString* link_name)
{
    QString directory = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation).first();
    if (directory.isEmpty()) {
        qWarning() << "failed to locate app local data directory" << directory;
        return false;
    }
    if (!createDirIfNotExists(directory)) {
        qWarning() << "failed to create directory" << directory;
        return false;
    }
    *link_name = pathJoin(directory, getBaseName(path));
    if (!QFileInfo(*link_name).isSymLink()) {
        if (!QFile::link(path, *link_name)) {
            qWarning() << "failed to create link from" << *link_name << "to" << path;
            return false;
        }
    }
    return true;
}

bool addFinderFavoriteDir(const QString &_path)
{
    QString link_name;
    if (!createLocalSymLink(_path, &link_name)) {
        return false;
    }

    NSString *path = [NSString stringWithUTF8String:link_name.toUtf8().data()];
    LSSharedFileListRef favList =
        LSSharedFileListCreate(NULL, kLSSharedFileListFavoriteItems, NULL);
    if (!favList) {
        return false;
    }

    CFArrayRef favListArray = LSSharedFileListCopySnapshot(favList, NULL);
    bool exists = false;
    const UInt32 resolutionFlags = kLSSharedFileListNoUserInteraction | kLSSharedFileListDoNotMountVolumes;
    for (id item in (__bridge NSArray *)favListArray) {
        LSSharedFileListItemRef itemRef = (__bridge LSSharedFileListItemRef)item;

        CFURLRef itemURL = NULL;

#if (__MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_10)
        itemURL = LSSharedFileListItemCopyResolvedURL(itemRef, resolutionFlags, NULL);
#else
        OSStatus err = LSSharedFileListItemResolve(itemRef, resolutionFlags, &itemURL, NULL);
        if (err != noErr) {
            if (itemURL) {
                CFRelease(itemURL);
            }
            continue;
        }
#endif
        if (itemURL != NULL) {
            if ([[(__bridge NSURL *)itemURL path] isEqualToString:path]) {
                exists = true;
            }
            CFRelease(itemURL);
            if (exists) {
                break;
            }
        }
    }
    if (favListArray != NULL) {
        CFRelease(favListArray);
    }

    if (exists) {
        qWarning() << "finder favorite list item for" << _path << "already exists";
        CFRelease(favList);
        return true;
    } else {
        NSURL *url = [NSURL fileURLWithPath:path];
        LSSharedFileListItemRef item =
            LSSharedFileListInsertItemURL(favList,
                                          kLSSharedFileListItemLast,
                                          NULL,
                                          NULL,
                                          (__bridge CFURLRef)url,
                                          NULL,
                                          NULL);
        qWarning() << "Added finder favorite list item for" << _path;
        CFRelease(favList);
        if (item) {
            CFRelease(item);
            return true;
        } else {
            return false;
        }
    }
}

bool enableSpotlightOnVolume(const QString& volume)
{
    const QString kMDUtilBinary = "mdutil";
    QStringList arguments {"-s", volume};
    QString output;
    int status = runAsCommand(kMDUtilBinary, arguments, &output);
    qDebug() << "output of \"mdutil -s\":" << output;

    // Typical output:
    // $ mdutil -s /Users/lin/SeaDrive
    // /Users/foo/SeaDrive:
    //         Indexing enabled.
    if (status == 0 && output.contains("enabled")) {
        qWarning("spotlight already enabled");
        return true;
    } else {
        QStringList arguments = {"-i", "on", volume};
        int ret = runAsCommand(kMDUtilBinary, arguments, &output);
        qWarning("enabled spotlight: %s", ret == 0 ? "successfully" : "failure");
        qDebug() << "output of \"mdutil -i on \":" << output;
        return ret == 0;
    }
    return true;
}

} // namespace mac
} // namespace utils
