#import "helper.h"

#import <MPMessagePack/MPXPCProtocol.h>

@implementation HelperService

+ (int)run {
  NSString *version =[[[NSBundle mainBundle] infoDictionary] valueForKey:@"CFBundleShortVersionString"];

  NSLog(@"Starting seadrive helper: %@", version);

  xpc_connection_t service = xpc_connection_create_mach_service("com.seafile.seadrive.helper", dispatch_get_main_queue(), XPC_CONNECTION_MACH_SERVICE_LISTENER);
  if (!service) {
    NSLog(@"Failed to create service.");
    return EXIT_FAILURE;
  }

  @try {
    HelperService *helper = [[HelperService alloc] init];
    [helper listen:service];

    dispatch_main();
  } @catch(NSException *e) {
    NSLog(@"Exception: %@", e);
  }

  return 0;
}

- (void)handleRequestWithMethod:(NSString *)method params:(NSArray *)params messageId:(NSNumber *)messageId completion:(void (^)(NSError *error, id value))completion {
  // @try {
  //   [self _handleRequestWithMethod:method params:params messageId:messageId completion:completion];
  // } @catch (NSException *e) {
  //   NSLog(@"Exception: %@", e);
  //   completion(KBMakeError(MPXPCErrorCodeInvalidRequest, @"Exception: %@", e), nil);
  // }
}

@end
