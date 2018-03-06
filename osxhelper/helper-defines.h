#import <Foundation/Foundation.h>

#define MakeError(CODE, MSG, ...)                                           \
    [NSError errorWithDomain:@"Keybase"                                     \
                        code:CODE                                           \
                    userInfo:@{                                             \
                        NSLocalizedDescriptionKey :                         \
                            [NSString stringWithFormat:MSG, ##__VA_ARGS__], \
                        NSLocalizedRecoveryOptionsErrorKey : @[ @"OK" ]     \
                    }]
