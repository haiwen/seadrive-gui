// -*- mode: objc -*-
#import <Foundation/Foundation.h>

@interface NSDictionary (SFJSON)

+ (instancetype)SFJSONLoads:(NSString *)data;
- (NSString *)SFJSONDumps;

@end
