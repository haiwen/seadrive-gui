// -*- mode: objc -*-

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <QuickLook/QuickLook.h>

#ifdef __cplusplus
extern "C"
{
#endif

@interface SysPlugin: NSObject
@end


// This class is responsible for:
//
// 1. finding / loading all system quicklook generators
// 2. Invoke proper system quicklook generator for the given url / uti
@interface SystemQLGen: NSObject
+ (instancetype)sharedInstance;
- (OSStatus)genThumnail:(QLThumbnailRequestRef)thumbnail
                    url:(CFURLRef)url
         contentTypeUTI:(CFStringRef)contentTypeUTI
                options:(CFDictionaryRef)options
                maxSize:(CGSize)maxSize;
- (OSStatus)genPreview:(QLPreviewRequestRef)preview
                    url:(CFURLRef)url
         contentTypeUTI:(CFStringRef)contentTypeUTI
                options:(CFDictionaryRef)options;

// Exposed for cli
- (BOOL)loadAllSystemGenerators;
- (SysPlugin *)findPluginForUTI:(NSString *)currentUTI;
@end

#ifdef __cplusplus
}
#endif
