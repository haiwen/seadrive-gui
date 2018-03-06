#import <MPMessagePack/MPXPCProtocol.h>

#import "helper-defines.h"
#import "helper.h"

@implementation HelperService

+ (int)run
{
    NSString *version = [[[NSBundle mainBundle] infoDictionary]
        valueForKey:@"CFBundleShortVersionString"];

    NSLog(@"Starting seadrive helper: %@", version);

    xpc_connection_t service = xpc_connection_create_mach_service(
        "com.seafile.seadrive.helper",
        dispatch_get_main_queue(),
        XPC_CONNECTION_MACH_SERVICE_LISTENER);
    if (!service) {
        NSLog(@"Failed to create service.");
        return EXIT_FAILURE;
    }

    @try {
        HelperService *helper = [[HelperService alloc] init];
        [helper listen:service];

        dispatch_main();
    } @catch (NSException *e) {
        NSLog(@"Exception: %@", e);
    }

    return 0;
}

- (void)handleRequestWithMethod:(NSString *)method
                         params:(NSArray *)params
                      messageId:(NSNumber *)messageId
                     completion:(void (^)(NSError *error, id value))completion
{
    @try {
        [self _handleRequestWithMethod:method
                                params:params
                             messageId:messageId
                            completion:completion];
    } @catch (NSException *e) {
        NSLog(@"Exception: %@", e);
        completion(
            MakeError(MPXPCErrorCodeInvalidRequest, @"Exception: %@", e),
            nil);
    }
}

- (void)_handleRequestWithMethod:(NSString *)method
                          params:(NSArray *)params
                       messageId:(NSNumber *)messageId
                      completion:(void (^)(NSError *error, id value))completion
{
    NSDictionary *args = [params count] == 1 ? params[0] : @{};
    NSLog(@"Request: %@(%@)", method, args);
    if ([method isEqualToString:@"version"]) {
        [self version:completion];
    } else {
        completion(MakeError(MPXPCErrorCodeUnknownRequest, @"Unknown request method"), nil);
    }
}

- (void)version:(void (^)(NSError *error, id value))completion {
  NSString *version = [NSBundle.mainBundle.infoDictionary valueForKey:@"CFBundleShortVersionString"];
  NSDictionary *response = @{
                             @"version": version,
                             };
  completion(nil, response);
}

@end
