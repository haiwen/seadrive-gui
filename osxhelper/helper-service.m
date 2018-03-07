#import <MPMessagePack/MPXPCProtocol.h>

#import "helper-defines.h"
#import "helper-kext.h"
#import "helper-log.h"
#import "helper-service.h"

@implementation HelperService

+ (int)run
{
    NSString *version =
        NSBundle.mainBundle.infoDictionary[@"CFBundleShortVersionString"];

    HelperLog(@"Starting seadrive helper: %@", version);

    xpc_connection_t service = xpc_connection_create_mach_service(
        "com.seafile.seadrive.helper",
        dispatch_get_main_queue(),
        XPC_CONNECTION_MACH_SERVICE_LISTENER);
    if (!service) {
        HelperLog(@"Failed to create service.");
        return EXIT_FAILURE;
    }

    @try {
        HelperService *helper = [[HelperService alloc] init];
        [helper listen:service];

        dispatch_main();
    } @catch (NSException *e) {
        HelperLog(@"Exception: %@", e);
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
        HelperLog(@"Exception: %@", e);
        completion(
            HelperMakeError(MPXPCErrorCodeInvalidRequest, @"Exception: %@", e),
            nil);
    }
}

- (void)_handleRequestWithMethod:(NSString *)method
                          params:(NSArray *)params
                       messageId:(NSNumber *)messageId
                      completion:(void (^)(NSError *error, id value))completion
{
    NSDictionary *args = [params count] == 1 ? params[0] : @{};
    HelperLog(@"Request: %@(%@)", method, args);
    if ([method isEqualToString:@"version"]) {
        [self version:completion];
    } else if ([method isEqualToString:@"kextInstall"]) {
        [HelperKext installWithSource:args[@"source"]
                          destination:args[@"destination"]
                               kextID:args[@"kextID"]
                             kextPath:args[@"kextPath"]
                           completion:completion];
    } else {
        completion(HelperMakeError(MPXPCErrorCodeUnknownRequest,
                                   @"Unknown request method"),
                   nil);
    }
}

- (void)version:(void (^)(NSError *error, id value))completion
{
    NSString *version =
        NSBundle.mainBundle.infoDictionary[@"CFBundleShortVersionString"];
    NSDictionary *response = @{
        @"version" : version,
    };
    completion(nil, response);
}

@end
