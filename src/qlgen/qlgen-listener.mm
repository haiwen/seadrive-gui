// #include "qlgen/qlgen-handler.h"
#include <QObject>
#include <QString>
#include <cstdint>
#include <vector>

#include <libkern/OSAtomic.h>
#include <mach/mach.h>
#import <Cocoa/Cocoa.h>

#import "qlgen/src/qlgen-proto.h"
#import "qlgen/src/NSDictionary+SFJSON.h"

#include "utils/utils.h"
#include "utils/stl.h"
#include "seadrive-gui.h"
#include "qlgen-handler.h"
#include "qlgen-listener.h"

#if !__has_feature(objc_arc)
#error this file must be built with ARC support
#endif

namespace {

NSThread *qlgen_listener_thread = nil;
volatile int32_t qlgen_started = 0;

struct QtLaterDeleter {
public:
  void operator()(QObject *ptr) {
    ptr->deleteLater();
  }
};

static NSDictionary *simpleResponse(NSObject *obj)
{
    NSDictionary *dict =
        [NSDictionary dictionaryWithObjectsAndKeys:obj, @"response", nil];
    return dict;
}

static NSDictionary *simpleError(NSString *msg)
{
    NSDictionary *dict =
        [NSDictionary dictionaryWithObjectsAndKeys:msg, @"error", nil];
    return dict;
}

static NSDictionary *missingArgsError(const char *arg)
{
    return simpleError([NSString stringWithFormat:@"missing argument \"%s\"", arg]);
}

BOOL hasSameInfoPlist(NSString *bundle1, NSString *bundle2)
{
    NSDictionary * (^loadBundleInfoDict)(NSString *) = ^(NSString *bundlePath) {
        NSString *infoPlistPath = [bundlePath stringByAppendingPathComponent:@"Contents/Info.plist"];
        NSURL *url = [[NSURL alloc] initFileURLWithPath:infoPlistPath];
        return [NSDictionary dictionaryWithContentsOfURL:url];
    };

    NSDictionary *infoDict1 = loadBundleInfoDict(bundle1);
    NSDictionary *infoDict2 = loadBundleInfoDict(bundle2);
    return infoDict1 && infoDict2 && [infoDict1 isEqualToDictionary:infoDict2];
}

BOOL makeDirs(NSString *path)
{
    if ([NSFileManager.defaultManager fileExistsAtPath:path]) {
        return YES;
    }

    NSError *error;
    if (![[NSFileManager defaultManager] createDirectoryAtPath:path
                                   withIntermediateDirectories:YES
                                                    attributes:nil
                                                         error:&error]) {
        NSString *err = [NSString stringWithFormat:@"%@", error];
        qWarning("failed to create dir %s : %s", path.UTF8String, err.UTF8String);
        return NO;
    }
    return YES;
}

BOOL installQLGeneratorImpl()
{
    NSString *appdir = [[NSBundle mainBundle] bundlePath];
    NSString *appQLGenDir = [appdir stringByAppendingPathComponent:@"Contents/Resources/SeaDriveQL.qlgenerator"];
    if (![NSFileManager.defaultManager fileExistsAtPath:appQLGenDir]) {
        qWarning("ql generator not found: %s\n", appQLGenDir.UTF8String);
        return NO;
    }
    NSString *userQLDir = [@"~/Library/QuickLook" stringByExpandingTildeInPath];
    if (!makeDirs(userQLDir)) {
        return NO;
    }

    NSString *dst = [userQLDir stringByAppendingPathComponent:@"SeaDriveQL.qlgenerator"];
    if (hasSameInfoPlist(appQLGenDir, dst)) {
        qWarning("Info.plist in %s and %s are the same. Nothing to do", appQLGenDir.UTF8String, dst.UTF8String);
        return YES;
    }

    NSError *error;
    if ([NSFileManager.defaultManager fileExistsAtPath:dst]) {
        if (![NSFileManager.defaultManager removeItemAtPath:dst error:&error]) {
            NSString *err = [NSString stringWithFormat:@"%@", error];
            qWarning("failed to remove existing %s : %s", dst.UTF8String, err.UTF8String);
            return NO;
        }
        qWarning("Removed old ql generator at %s", dst.UTF8String);
    }
    if (![NSFileManager.defaultManager copyItemAtPath:appQLGenDir toPath:dst error:&error]) {
        NSString *err = [NSString stringWithFormat:@"%@", error];
        qWarning("failed to copy %s => %s : %s", appQLGenDir.UTF8String, dst.UTF8String, err.UTF8String);
        return NO;
    } else {
        qWarning("Copied %s => %s", appQLGenDir.UTF8String, dst.UTF8String);
    }
    // Tell the system to re scan the ql generators
    qWarning("Asking quicklookd to refresh generators list");
    QProcess::execute("qlmanage", QStringList("-r"));
    return YES;
}

void installQLGenerator()
{
    // Do the real work in a worker thread to avoid blocking on the
    // main thread.
    dispatch_async(
        dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0),
        ^{ installQLGeneratorImpl(); }
    );
}

} // anonymous namespace

@interface QLGenListener : NSObject <NSMachPortDelegate>
@property QLGenHandler *handler;
@property NSPort *listenerPort;

+(instancetype)sharedInstance;
- (void)handleMachMessage:(void *)machMessage;
-(NSDictionary *)handleOneRequest:(NSDictionary *)request;
@end

@implementation QLGenListener
+ (id)sharedInstance
{
    static QLGenListener *instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
      instance = [QLGenListener new];
    });
    return instance;
}
- (instancetype)init {
    _listenerPort = nil;
    _handler = new QLGenHandler();
    return self;
}
- (void)start {
    self.handler->start();
    NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
    mach_port_t port = MACH_PORT_NULL;

    kern_return_t kr =
        mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port);
    if (kr != KERN_SUCCESS) {
        qWarning("[QLGen] failed to allocate mach port");
        qDebug("[QLGen] mach error %s", mach_error_string(kr));
        kr = mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE,
                                -1);
        if (kr != KERN_SUCCESS) {
            qDebug("[QLGen] failed to deallocate mach port");
            qDebug("[QLGen] mach error %s", mach_error_string(kr));
        }
        return;
    }

    kr = mach_port_insert_right(mach_task_self(), port, port,
                                MACH_MSG_TYPE_MAKE_SEND);
    if (kr != KERN_SUCCESS) {
        qWarning("[QLGen] failed to insert send right to mach port");
        qDebug("[QLGen] mach error %s", mach_error_string(kr));
        kr = mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE,
                                -1);
        if (kr != KERN_SUCCESS) {
            qDebug("[QLGen] failed to deallocate mach port");
            qDebug("[QLGen] mach error %s", mach_error_string(kr));
        }
        qDebug("[QLGen] failed to allocate send right tp local mach port");
        return;
    }
    self.listenerPort =
        [NSMachPort portWithMachPort:port
                             options:NSMachPortDeallocateReceiveRight];
    if (![[NSMachBootstrapServer sharedInstance]
            registerPort:self.listenerPort
                    name:[NSString stringWithUTF8String:kSFQLGenMachPort]]) {
        [self.listenerPort invalidate];
        qWarning("[QLGen] failed to register mach port");
        qDebug("[QLGen] mach error %s", mach_error_string(kr));
        return;
    }
    qWarning("[QLGen] mach port registered");
    [self.listenerPort setDelegate:self];
    [runLoop addPort:self.listenerPort forMode:NSDefaultRunLoopMode];
    while (qlgen_started)
        [runLoop runMode:NSDefaultRunLoopMode
              beforeDate:[NSDate distantFuture]];
    [self.listenerPort invalidate];
    qWarning("[QLGen] mach port unregistered");
    kr = mach_port_deallocate(mach_task_self(), port);
    if (kr != KERN_SUCCESS) {
        qDebug("[QLGen] failed to deallocate mach port %u", port);
        return;
    }
}
- (void)stop {
    CFRunLoopStop(CFRunLoopGetCurrent());
}

// Helper class to ease the clean up of mach msgs.
class MachMsgDestroyer
{
public:
    MachMsgDestroyer(mach_msg_header_t *header) : header_(header){};
    ~MachMsgDestroyer()
    {
        mach_msg_destroy(header_);
    }

private:
    mach_msg_header_t *header_;
};

- (void)handleMachMessage:(void *)machMessage {
    SFQLGenRequest *request =
        static_cast<SFQLGenRequest *>(machMessage);
    MachMsgDestroyer request_destroyer(&request->header);

    if (request->header.msgh_size != sizeof(SFQLGenReply)) {
        qWarning("[QLGen] received msg with bad size %u", request->header.msgh_size);
        return;
    }

    qDebug("Got request body: %s\n", request->body);
    mach_port_t port = request->header.msgh_remote_port;
    if (!port) {
        qWarning("no reply port found in incoming mach msg, must be a bug\n");
        return;
    }

    NSDictionary *requestDict = [NSDictionary
        SFJSONLoads:[NSString stringWithUTF8String:request->body]];
    if (!requestDict) {
        return;
    }

    NSDictionary *replyDict = [self handleOneRequest:requestDict];
    if (!replyDict) {
        return;
    }

    ZERO_INIT(SFQLGenReply, reply);
    mach_msg_header_t *reply_header =
      reinterpret_cast<mach_msg_header_t*>(&reply);
    MachMsgDestroyer reply_destroyer(reply_header);

    reply_header->msgh_size = sizeof(SFQLGenReply);
    reply_header->msgh_local_port = MACH_PORT_NULL;
    reply_header->msgh_remote_port = port;
    reply_header->msgh_bits = MACH_MSGH_BITS_REMOTE(request->header.msgh_bits);

    strcpy(reply.body, [replyDict SFJSONDumps].UTF8String);

    // send the reply
    kern_return_t kr = mach_msg_send(reply_header);
    if (kr != MACH_MSG_SUCCESS) {
        qDebug("[QLGen] mach error %s", mach_error_string(kr));
        qWarning("[QLGen] failed to send reply");
    } else {
        // qWarning("[QLGen] reply sent");
    }

    mach_msg_destroy(reply_header);
}

-(NSDictionary *)handleOneRequest:(NSDictionary *)request
{
    NSString *cmd = [request valueForKey:@"command"];
    if (!cmd) {
        qWarning("invalid request");
        return missingArgsError("command");
    }
    qDebug("Got command: %s\n", cmd.UTF8String);

    auto isCmd = ^(const char *target){
        return [cmd isEqualToString:[NSString stringWithUTF8String:target]];
    };

    // Command dispatching
    if (isCmd("getMountPoint")) {
        return simpleResponse(gui->mountDir().toNSString());
    } else if (isCmd("isFileCached")) {
        NSString *path = [request valueForKey:@"path"];
        if (!path) {
            return missingArgsError("path");
        }
        bool cached = self.handler->isFileCached(QString::fromNSString(path));
        return simpleResponse([NSNumber numberWithBool:cached]);
    } else if (isCmd("fetchThumbnail")) {
        NSString *path = [request valueForKey:@"path"];
        NSNumber *sizeObj = [request valueForKey:@"size"];
        if (!path) {
            return missingArgsError("path");
        }
        if (!sizeObj) {
            return missingArgsError("size");
        }
        if (sizeObj.intValue <= 0) {
            return simpleError(@"invalid size");
        }
        // Adjust the size properly for retina display, i.e. for a
        // 16x16 thumb it would actually. require 32x32.
        // Use qMax because sometimes `backingScaleFactor` could return 0.
        int scaleFactor = qMax(1.0, [[NSScreen mainScreen] backingScaleFactor]);
        int size = sizeObj.intValue * scaleFactor;

        QString output;
        if (self.handler->fetchThumbnail(QString::fromNSString(path), size, &output)) {
            return simpleResponse(output.toNSString());
        } else {
            return simpleError(@"failed to fetch, check seadrive gui logs");
        }
    }

    qWarning("invalid command: %s\n", cmd.UTF8String);
    return simpleError(
        [NSString stringWithFormat:@"invalid command \"%@\"", cmd]);
}

@end

void qlgenListenerStart() {
    installQLGenerator();
    if (!OSAtomicAdd32Barrier(0, &qlgen_started)) {
        OSAtomicIncrement32Barrier(&qlgen_started);

        dispatch_async(dispatch_get_main_queue(), ^{
          QLGenListener *qlgen_listener = [QLGenListener sharedInstance];
          qlgen_listener_thread =
              [[NSThread alloc] initWithTarget:qlgen_listener
                                      selector:@selector(start)
                                        object:nil];
          [qlgen_listener_thread start];
        });
    }
}

void qlgenListenerStop() {
    if (OSAtomicAdd32Barrier(0, &qlgen_started)) {
        OSAtomicDecrement32Barrier(&qlgen_started);

        QLGenListener *qlgen_listener = [QLGenListener sharedInstance];
        [qlgen_listener performSelector:@selector(stop)
                                      onThread:qlgen_listener_thread
                                    withObject:nil
                                 waitUntilDone:NO];
    }
}
