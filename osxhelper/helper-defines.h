#import <Foundation/Foundation.h>

typedef NS_ENUM(NSInteger, HelperError) {
  HelperErrorKext = -1000,
};

typedef void (^HelperCompletion)(NSError *error);
typedef void (^HelperOnCompletion)(NSError *error, id value);

#define HelperMakeError(CODE, MSG, ...) [NSError errorWithDomain:@"alphadrive" code:CODE userInfo:@{NSLocalizedDescriptionKey:[NSString stringWithFormat:MSG, ##__VA_ARGS__], NSLocalizedRecoveryOptionsErrorKey: @[@"OK"]}]

#define HelperOr(obj, dv) (obj ? obj : dv)
#define HelperIfNull(obj, val) ([obj isEqual:NSNull.null] ? val : obj)

NSString *HelperNSStringWithFormat(NSString *formatString, ...);
