// -*- mode: objc -*-

#import <Foundation/Foundation.h>
#import <QuickLook/QuickLook.h>

#ifdef __cplusplus
extern "C"
{
#endif

@protocol SFQLGen <NSObject>
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

// These methods are exposed so we can quickly test them with the cli
// See qlgen/cli/cli.m
- (BOOL)isFileCached:(NSString *)path output:(BOOL *)output;
- (BOOL)getMountPoint:(NSString **)mountPoint;
- (BOOL)askForThumbnail:(NSString *)path size:(CGFloat)size output:(NSString **)output;
@end

id<SFQLGen> getDefaultSFQLGen();

#ifdef __cplusplus
}
#endif
