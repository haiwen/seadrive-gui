#include <AvailabilityMacros.h>
#import <Cocoa/Cocoa.h>

#import "helper-client.h"

#if !__has_feature(objc_arc)
#error this file must be built with ARC support
#endif

HelperClient::HelperClient()
{
    xpc_client_ = nullptr;
}

void HelperClient::connect()
{
    xpc_client_ = [[MPXPCClient alloc]
        initWithServiceName:@"com.seafile.seadrive.helper"
                 privileged:YES
                readOptions:MPMessagePackReaderOptionsUseOrderedDictionary];
    xpc_client_.retryMaxAttempts = 4;
    xpc_client_.retryDelay = 0.5;
    xpc_client_.timeout = 10.0;
}

void HelperClient::getVersion()
{
    if (!xpc_client_) {
        connect();
    }
    [xpc_client_ sendRequest:@"version"
                      params:nil
                  completion:^(NSError *error, NSDictionary *versions) {
                    if (error) {
                        printf("get version error!\n");
                        NSLog(@"error: %@", [error userInfo]);
                    } else {
                        printf("get version success!\n");
                        NSLog(@"Helper version: %@", versions);
                    }
                  }];
}
