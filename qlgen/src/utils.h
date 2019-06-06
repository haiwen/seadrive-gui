#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef DEBUG
// Use this `DbgLog` macro for inserting debug messages when
// development. It is a no-op when the project is compiled in Release
// mode.
    #define DbgLog(...) NSLog(__VA_ARGS__)
#else
    #define DbgLog(...)
#endif

#define myDomain @"com.seafile.seadriveql"

NSString *SFURLToPath(NSURL *url);

NSURL *SFPathToURL(NSString *path);

#ifdef __cplusplus
}
#endif
