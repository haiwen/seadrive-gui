#include <mach/mach.h>
#include <servers/bootstrap.h>
#import <Cocoa/Cocoa.h>
#import "utils.h"
#import "qlgen-proto.h"
#import "qlgen-client.h"
#import "NSDictionary+SFJSON.h"

namespace {

const mach_msg_timeout_t kDefaultTimeoutInMSecs = 1000;
const mach_msg_timeout_t kTimeoutForFetchThumbnailRequestInMSecs = 20000;

typedef struct {
    SFQLGenReply inner;
    // not used but added by the kernel anyway
    mach_msg_trailer_t trailer;
} SFQLGenReplyForReceive;

} // namespace

@interface SFQLGenClient ()
// Local port for receiving replies
@property mach_port_t localPort;
// Remote port for sending requests to the server
@property mach_port_t remotePort;

- (BOOL)initPorts;
- (BOOL)initRemotePort:(BOOL)reconnect;
-(BOOL)sendRequest:(NSDictionary *)req;
-(BOOL)sendWithRetry:(mach_msg_header_t *)header withRetry:(BOOL)shouldRetry;
-(BOOL)readReply:(NSObject **)buf;
-(BOOL)readReplyWithTimeout:(NSObject **)buf timeoutInMillSeconds:(mach_msg_timeout_t)timeout;
@end

@implementation SFQLGenClient
- (BOOL)initPorts
{
    if (!_localPort) {
        kern_return_t kr = mach_port_allocate(
            mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &_localPort);
        if (kr != KERN_SUCCESS) {
            NSLog(@"unable to create local port");
            return NO;
        }
    }

    return [self initRemotePort:NO];
}

- (BOOL)initRemotePort:(BOOL)reconnect
{
    if (!reconnect && _remotePort) {
        // remote port is already ready
        return YES;
    }

    // Take care of deallocating existing _remotePort when
    // reconnecting
    if (_remotePort) {
        mach_port_deallocate(mach_task_self(), _remotePort);
    }
    kern_return_t kr = bootstrap_look_up(
        bootstrap_port,
        kSFQLGenMachPort,
        &_remotePort);

    if (kr != KERN_SUCCESS) {
        NSLog(@"failed to connect to qlgen server");
        return NO;
    }
    NSLog(@"connected to qlgen server");
    return YES;
}

- (BOOL)getMountPoint:(NSString **)mountPoint
{
    if (![self initPorts]) {
        return NO;
    }
    NSDictionary *req = [NSDictionary
        dictionaryWithObjectsAndKeys:@"getMountPoint", @"command", nil];
    if (![self sendRequest:req]) {
        return NO;
    }
    if (![self readReply:mountPoint]) {
        return NO;
    }
    return YES;
}

- (BOOL)isFileCached:(NSString *)path output:(BOOL *)output
{
    if (![self initPorts]) {
        return NO;
    }
    NSDictionary *req =
        [NSDictionary dictionaryWithObjectsAndKeys:@"isFileCached",
                                                   @"command",
                                                   path,
                                                   @"path",
                                                   nil];
    if (![self sendRequest:req]) {
        return NO;
    }
    NSNumber *cached = nil;
    if (![self readReply:&cached]) {
        return NO;
    }
    *output = [cached boolValue];
    return YES;
}

-(BOOL)sendWithRetry:(mach_msg_header_t *)header withRetry:(BOOL)shouldRetry
{
    header->msgh_local_port = _localPort;
    header->msgh_remote_port = _remotePort;
    header->msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, // remote
                                       MACH_MSG_TYPE_MAKE_SEND); // local
    // send a message and wait for the reply
    kern_return_t kr = mach_msg(header,
                                MACH_SEND_MSG | MACH_SEND_TIMEOUT, /*option*/
                                header->msgh_size, /*send size*/
                                0,               /*receive size*/
                                _localPort,     /*receive port*/
                                100,             /*timeout, in milliseconds*/
                                MACH_PORT_NULL); /*no notification*/
    if (kr != MACH_MSG_SUCCESS) {
        NSLog(@"failed to send request to SeaDrive Client: %s", mach_error_string(kr));
        if (shouldRetry && [self initRemotePort:YES]) {
            return [self sendWithRetry:header withRetry:NO];
        }
        return NO;
    }
    return YES;
}

-(BOOL)sendRequest:(NSDictionary *)req
{
    NSString *body = [req SFJSONDumps];
    ZERO_INIT(SFQLGenRequest, msg);
    msg.header.msgh_size = sizeof(msg);
    strcpy(msg.body, [body UTF8String]);
    return [self sendWithRetry:&msg.header withRetry:YES];
}

-(BOOL)readReply:(NSObject **)buf
{
    return [self readReplyWithTimeout:buf timeoutInMillSeconds:kDefaultTimeoutInMSecs];
}

-(BOOL)readReplyWithTimeout:(NSObject **)buf timeoutInMillSeconds:(mach_msg_timeout_t)timeout
{
    ZERO_INIT(SFQLGenReplyForReceive, received);
    SFQLGenReply *reply = &received.inner;
    mach_msg_header_t *recv_msg_header = (mach_msg_header_t *)reply;
    recv_msg_header->msgh_local_port = _localPort;
    recv_msg_header->msgh_remote_port = _remotePort;
    kern_return_t kr;
    kr = mach_msg(recv_msg_header,                                  /* header*/
                  MACH_RCV_MSG | MACH_RCV_TIMEOUT | MACH_RCV_LARGE, /*option*/
                  0,               /*send size*/
                  sizeof(SFQLGenReplyForReceive), /*receive size*/
                  _localPort,     /*receive port*/
                  timeout,        /*timeout, in milliseconds*/
                  MACH_PORT_NULL); /*no notification*/

    if (kr == MACH_RCV_TOO_LARGE) {
        NSLog(@"expected %lu bytes, but got %u bytes", sizeof(SFQLGenReplyForReceive), recv_msg_header->msgh_size);
        return NO;
    }
    if (kr != MACH_MSG_SUCCESS) {
        NSLog(@"failed to receive SeaDrive Client's reply, mach error %s", mach_error_string(kr));
        // connectionBecomeInvalid();
        return NO;
    }
    DbgLog(@"Got reply: %s", reply->body);
    NSDictionary *replyDict =
        [NSDictionary SFJSONLoads:[NSString stringWithUTF8String:reply->body]];
    if (!replyDict) {
        NSLog(@"invalid reply %s", reply->body);
        return NO;
    }
    NSString *error = [replyDict objectForKey:@"error"];
    if (error) {
        NSLog(@"qlgen-client: error from gui: \"%@\"", error);
        return NO;
    }
    NSObject *value = [replyDict objectForKey:@"response"];
    if (!value) {
        return NO;
    }
    *buf = value;
    return YES;
}

- (BOOL)askForThumbnail:(NSString *)path size:(int)size output:(NSString **)output
{
    if (![self initPorts]) {
        return NO;
    }
    NSNumber *sizeObj = [NSNumber numberWithInt:size];
    NSDictionary *req =
        [NSDictionary dictionaryWithObjectsAndKeys:@"fetchThumbnail",
                                                   @"command",
                                                   path,
                                                   @"path",
                                                   sizeObj,
                                                   @"size",
                                                   nil];
    if (![self sendRequest:req]) {
        return NO;
    }
    NSString *thumbFile = nil;
    // Use a larger timeout for fetching thumbnail since it involves
    // sending api requests to seahub.
    if (![self readReplyWithTimeout:&thumbFile
               timeoutInMillSeconds:kTimeoutForFetchThumbnailRequestInMSecs]) {
        return NO;
    }
    *output = thumbFile;
    return YES;
}

@end
