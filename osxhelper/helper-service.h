#import <Foundation/Foundation.h>

#import <MPMessagePack/MPMessagePack.h>
#import <MPMessagePack/MPXPCService.h>

@interface HelperService : MPXPCService

+ (int)run;

@end
