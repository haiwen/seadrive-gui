#include <getopt.h>
#import <servers/bootstrap.h>
#import <Cocoa/Cocoa.h>

#if !__has_feature(objc_arc)
#error this file must be built with ARC support
#endif

#import "utils.h"
#import "qlgen.h"
#import "system-qlgen.h"

static mach_port_t local_port_;
static mach_port_t remote_port_;

#define ZERO_INIT(TYPE, VAR)                    \
    TYPE VAR;                                   \
    bzero(&VAR, sizeof(TYPE));

typedef struct {
    mach_msg_header_t header;
    char body[30];
    // not used
    mach_msg_trailer_t trailer;
} SFQLGenReply;

BOOL readReply() {
    ZERO_INIT(SFQLGenReply, reply);
    mach_msg_header_t *recv_msg_header = (mach_msg_header_t *)&reply;
    recv_msg_header->msgh_local_port = local_port_;
    recv_msg_header->msgh_remote_port = remote_port_;
    kern_return_t kr;
    // receive the reply
    kr = mach_msg(recv_msg_header,                                  /* header*/
                  MACH_RCV_MSG | MACH_RCV_TIMEOUT | MACH_RCV_LARGE, /*option*/
                  0,               /*send size*/
                  sizeof(SFQLGenReply), /*receive size*/
                  local_port_,     /*receive port*/
                  1000,             /*timeout, in milliseconds*/
                  MACH_PORT_NULL); /*no notification*/

    if (kr == MACH_RCV_TOO_LARGE) {
        NSLog(@"expected %lu bytes, but got %u bytes", sizeof(SFQLGenReply), recv_msg_header->msgh_size);
        return NO;
    }
    // // retry
    // if (kr == MACH_RCV_TOO_LARGE) {
    //     recv_msg.resize(recv_msg_header->msgh_size +
    //                     sizeof(mach_msg_trailer_t));
    //     recv_msg_header =
    //         reinterpret_cast<mach_msg_header_t *>(recv_msg.data());

    //     kr = mach_msg(recv_msg_header,                 /* header*/
    //                   MACH_RCV_MSG | MACH_RCV_TIMEOUT, /*option*/
    //                   0,                               /*send size*/
    //                   recv_msg.size(),                 /*receive size*/
    //                   local_port_,                     /*receive port*/
    //                   100,             /*timeout, in milliseconds*/
    //                   MACH_PORT_NULL); /*no notification*/
    // }
    if (kr != MACH_MSG_SUCCESS) {
        NSLog(@"failed to receive SeaDrive Client's reply");
        NSLog(@"mach error %s", mach_error_string(kr));
        // connectionBecomeInvalid();
        return NO;
    }

    // if (recv_msg_header->msgh_id != recv_msgh_id) {
    //     NSLog(@"mach error unmatched message id %d, expected %d",
    //           recv_msg_header->msgh_id, recv_msgh_id);
    //     connectionBecomeInvalid();
    //     return;
    // }
    NSLog(@"Got reply: %s", reply.body);
    return YES;
}

static int getMountPoint(id<SFQLGen> qlgen)
{
    NSString *mountPoint = nil;
    if ([qlgen getMountPoint:&mountPoint]) {
        NSLog(@"mount point is: %@", mountPoint);
        return 0;
    } else {
        NSLog(@"getMountPoint failed");
        return 1;
    }
}

static int findSystemPluginForUTI(const char *uti)
{
    SystemQLGen *sys = [SystemQLGen sharedInstance];
    [sys loadAllSystemGenerators];
    id matchedPlugin = [sys findPluginForUTI:[NSString stringWithUTF8String:uti]];
    if (!matchedPlugin) {
        NSLog(@"No sys plugin found for uti %s", uti);
    }
    return 0;
}

static int isFileCached(id<SFQLGen> qlgen, const char* path)
{
    BOOL isFileCached = NO;
    if ([qlgen isFileCached:[NSString stringWithUTF8String:path] output:&isFileCached]) {
        NSLog(@"file is %s", isFileCached ? "cached" : "not cached");
        return 0;
    } else {
        NSLog(@"isFileCached failed");
        return 1;
    }
}

static int askForThumbnail(id<SFQLGen> qlgen, const char* path)
{
    NSString *thumbFile;
    if ([qlgen askForThumbnail:[NSString stringWithUTF8String:path] size:14 output:&thumbFile]) {
        NSLog(@"Got api generated thumbnail at path %@", thumbFile);
        return 0;
    } else {
        NSLog(@"Failed to ask for thumbnail for file %s", path);
        return 1;
    }
}

static int run(int argc, char **argv)
{
    int c;
    static const char *short_options = "Ml:c:t:";
    static const struct option long_options[] = {
        { "get-mount-point", no_argument, NULL, 'M' },
        { "find-system-generators", required_argument, NULL, 'l'},
        { "is-file-cached", required_argument, NULL, 'c'},
        { "ask-for-thumbnail", required_argument, NULL, 't'},
        { NULL, 0, NULL, 0, },
    };

    id<SFQLGen> qlgen = getDefaultSFQLGen();
    while ((c = getopt_long (argc, argv, short_options,
                             long_options, NULL)) != EOF) {
        switch (c) {
        case 'M':
            getMountPoint(qlgen);
            usleep(12 * 1000 * 1000);
            return getMountPoint(qlgen);
            break;
        case 'l':
            return findSystemPluginForUTI(optarg);
            break;
        case 'c':
            return isFileCached(qlgen, optarg);
            break;
        case 't':
            return askForThumbnail(qlgen, optarg);
            break;
        default:
            exit(1);
        }
    }

    return 0;
}

int main(int argc, char **argv)
{
    @autoreleasepool {
        return run(argc, argv);
    }
}
