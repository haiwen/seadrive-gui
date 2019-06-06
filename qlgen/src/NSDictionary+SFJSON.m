#import "NSDictionary+SFJSON.h"

@implementation NSDictionary (SFJSON)

+ (instancetype)SFJSONLoads:(NSString *)str
{
    NSError *error;
    NSData *data = [str dataUsingEncoding:NSUTF8StringEncoding];

    NSDictionary *dict = [NSJSONSerialization JSONObjectWithData:data
                                                         options:kNilOptions
                                                           error:&error];
    if (!dict) {
        NSLog(@"%s: error: %@", __func__, error.localizedDescription);
        return nil;
    }
    return dict;
}

- (NSString *)SFJSONDumps
{
    NSError *error;
    NSData *jsonData =
        [NSJSONSerialization dataWithJSONObject:self
                                        options:(NSJSONWritingOptions)0
                                          error:&error];

    if (!jsonData) {
        NSLog(@"%s: error: %@", __func__, error.localizedDescription);
        return @"{}";
    } else {
        return [[NSString alloc] initWithData:jsonData
                                     encoding:NSUTF8StringEncoding];
    }
}
@end
