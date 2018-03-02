//
//  MPDefines.h
//  MPMessagePack
//
//  Created by Gabriel on 12/13/14.
//  Copyright (c) 2014 Gabriel Handford. All rights reserved.
//

#undef MPDebug
#define MPDebug(fmt, ...) do {} while(0)
#undef MPErr
#define MPErr(fmt, ...) do {} while(0)

#if DEBUG
#undef MPDebug
#define MPDebug(fmt, ...) NSLog((@"%s:%d: " fmt), __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)
#undef MPErr
#define MPErr(fmt, ...) NSLog((@"%s:%d: " fmt), __PRETTY_FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

typedef void (^MPCompletion)(NSError *error);

#define MPMakeError(CODE, MSG, ...) [NSError errorWithDomain:@"MPMessagePack" code:CODE userInfo:@{NSLocalizedDescriptionKey:[NSString stringWithFormat:MSG, ##__VA_ARGS__], NSLocalizedRecoveryOptionsErrorKey: @[@"OK"]}]

#define MPMakeErrorWithRecovery(CODE, MSG, RECOVERY, ...) [NSError errorWithDomain:@"MPMessagePack" code:CODE userInfo:@{NSLocalizedDescriptionKey: MSG, NSLocalizedRecoveryOptionsErrorKey: @[@"OK"], NSLocalizedRecoverySuggestionErrorKey:[NSString stringWithFormat:RECOVERY, ##__VA_ARGS__]}]


#define MPIfNull(obj, val) ([obj isEqual:NSNull.null] ? val : obj)

#define MPWeakObject(o) __typeof__(o) __weak
#define MPWeakSelf MPWeakObject(self)

// #ifndef NS_ENUM
// #define NS_ENUM(_type, _name) enum _name : _type _name; enum _name : _type
// #endif

// #ifndef NS_ENUM
// #define NS_OPTIONS(_type, _name) enum _name : _type _name; enum _name : _type
// #endif

// In CFAvailability.h
// Enums and Options
#if (__cplusplus && __cplusplus >= 201103L && (__has_extension(cxx_strong_enums) || __has_feature(objc_fixed_enum))) || (!__cplusplus && __has_feature(objc_fixed_enum))
  #define NS_ENUM(_type, _name) enum _name : _type _name; enum _name : _type
  #if (__cplusplus)
    #define NS_OPTIONS(_type, _name) _type _name; enum : _type
  #else
    #define NS_OPTIONS(_type, _name) enum _name : _type _name; enum _name : _type
  #endif
#else
  #define NS_ENUM(_type, _name) _type _name; enum
  #define NS_OPTIONS(_type, _name) _type _name; enum
#endif
