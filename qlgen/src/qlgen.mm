#import <Cocoa/Cocoa.h>

#import "utils.h"
#import "qlgen-client.h"
#import "system-qlgen.h"
#import "qlgen.h"

namespace {

const CGFloat kDefaultPreviewSize = 400;

}

@interface SFQLGenImpl : NSObject<SFQLGen>

@property SFQLGenClient *client;
@property (copy) NSString *mountPoint;
@property (nonatomic) NSSet *imageFormatsSupportedBySeahub;

+ (id)sharedInstance;
- (id)init;
- (BOOL)getMountPoint:(NSString **)mountPoint;
- (BOOL)isFileCached:(NSString *)path;
- (BOOL)isFileLocalOrCached:(NSString *)path;
- (BOOL)askForThumbnail:(NSString *)path size:(CGFloat)size output:(NSString **)output;
- (OSStatus)genThumnail:(QLThumbnailRequestRef)thumbnail
                    url:(CFURLRef)url
         contentTypeUTI:(CFStringRef)contentTypeUTI
                options:(CFDictionaryRef)options
                maxSize:(CGSize)maxSize;
- (OSStatus)genPreview:(QLPreviewRequestRef)preview
                    url:(CFURLRef)url
         contentTypeUTI:(CFStringRef)contentTypeUTI
                options:(CFDictionaryRef)options;
- (void)finishPreviewWithPNG:(QLPreviewRequestRef)preview pngPath:(NSString *)pngPath options:(CFDictionaryRef)options;
- (BOOL)isSupportedImageFile:(NSString *)path contentTypeUTI:(CFStringRef)contentTypeUTI;
@end

@implementation SFQLGenImpl
+ (id)sharedInstance
{
    static SFQLGenImpl *instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
      instance = [SFQLGenImpl new];
    });
    return instance;
}

- (id)init
{
    _client = [SFQLGenClient new];
    // Seahub uses PIL to extract thumbnails. The list of supported
    // image formats can be found here:
    // https://pillow.readthedocs.io/en/4.1.x/handbook/image-file-formats.html
    _imageFormatsSupportedBySeahub = [NSSet setWithObjects:@"png",
                                                           @"bmp",
                                                           @"eps",
                                                           @"gif",
                                                           @"icns",
                                                           @"ico",
                                                           @"jpeg",
                                                           @"jpg",
                                                           @"png",
                                                           @"psd",
                                                           @"tiff",
                                                           @"webp",
                                                           @"xpm",
                                                           @"xbm",
                                                           nil];
    return self;
}

- (BOOL)getMountPoint:(NSString **)mountPoint
{
    return [self.client getMountPoint:mountPoint];
}

- (BOOL)isFileCached:(NSString *)path output:(BOOL *)output
{
    return [self.client isFileCached:path output:output];
}

- (BOOL)isSupportedImageFile:(NSString *)path contentTypeUTI:(CFStringRef)contentTypeUTI
{
    // We also registers for types like PDF and docx (to prevent
    // Finder from auto downloading files of these types), but seahub
    // api doesn't support thumbnails for these types.
    if (!UTTypeConformsTo(contentTypeUTI, kUTTypeImage)) {
        return NO;
    }

    NSString *extension = path.pathExtension;
    return extension.length > 0 && [self.imageFormatsSupportedBySeahub
                                       containsObject:path.pathExtension.lowercaseString];
}

- (OSStatus)genThumnail:(QLThumbnailRequestRef)thumbnail
                    url:(CFURLRef)url
         contentTypeUTI:(CFStringRef)contentTypeUTI
                options:(CFDictionaryRef)options
                maxSize:(CGSize)maxSize
{
    NSString *path = ((__bridge NSURL *)url).path;

    DbgLog(@"SFQLGenImpl::genThumnail is called for %@", path);
    BOOL localOrCached = [self isFileLocalOrCached:path];
    if (QLThumbnailRequestIsCancelled(thumbnail)) {
        return noErr;
    }

    if (localOrCached) {
        DbgLog(@"calling system qlgen for file %@", path);
        SystemQLGen *system = [SystemQLGen sharedInstance];
        return [system genThumnail:thumbnail
                               url:url
                    contentTypeUTI:contentTypeUTI
                           options:options
                           maxSize:maxSize];
    } else {
        if (![self isSupportedImageFile:path contentTypeUTI:contentTypeUTI]) {
            // For non-cached non-image files, we simply discard the request.
            return noErr;
        }
        NSString *png;

        if ([self askForThumbnail:path size:maxSize.width output:&png]) {
            DbgLog(@"use api generated thumbnail at path %@", png);
            NSURL *pngURL = SFPathToURL(png);
            QLThumbnailRequestSetImageAtURL(thumbnail, (__bridge CFURLRef)pngURL, nil);
            return noErr;
        } else {
            DbgLog(@"Failed to ask for thumbnail for file %@", path);
        }
    }

    return noErr;
}

- (BOOL)isFileLocalOrCached:(NSString *)path
{
    // TODO: cached location of mount point should expire, in case the
    // mount point could be changed on searive-gui side.
    if (!self.mountPoint) {
        NSString *mountPoint;
        if (![self getMountPoint:&mountPoint]) {
            // If we fail to get the montpoint, then seadrive may be
            // not running. In such case we treat all files as local
            // files.
            return YES;
        }
        self.mountPoint = mountPoint;
    }
    return ![path hasPrefix:self.mountPoint] || [self isFileCached:path];
}

- (OSStatus)genPreview:(QLPreviewRequestRef)preview
                    url:(CFURLRef)url
         contentTypeUTI:(CFStringRef)contentTypeUTI
                options:(CFDictionaryRef)options
{
    NSString *path = ((__bridge NSURL *)url).path;

    DbgLog(@"SFQLGenImpl::genPreview is called for %@", path);

    NSString *pngPath;
    BOOL localOrCached = [self isFileLocalOrCached:path];
    if (localOrCached) {
        DbgLog(@"calling system qlgen for file %@", path);
        SystemQLGen *system = [SystemQLGen sharedInstance];
        if (QLPreviewRequestIsCancelled(preview)) {
            return noErr;
        }
        return [system genPreview:preview
                              url:url
                   contentTypeUTI:contentTypeUTI
                          options:options];
    } else {
        // We can't return empty response here directly, because
        // Finder has this undocumented behavior to disable a
        // qlgenerator if it fails to generate a preview for more than
        // 10 times. In console.app logs one would find message like this:
        //
        //   [QL] Too many problems with <QLGenerator ....qlgenerator>. Disabling it
        //
        // Thus, For non-cached files:
        //
        // 1. If the file is an image, we generate a large enough
        // thumbnail to display as the preview. This is what finder
        // would do if we return null in GeneratePreviewForURL.
        //
        // 2. Otherwise, we generate a placeholder preview.
        if ([self isSupportedImageFile:path contentTypeUTI:contentTypeUTI]) {
            if ([self askForThumbnail:path size:kDefaultPreviewSize output:&pngPath]) {
                DbgLog(@"use api generated thumbnail as preview at path %@", pngPath);
            } else {
                DbgLog(@"Failed to ask for thumbnail as preview for file %@", path);
            }
        }

        if (!pngPath) {
            DbgLog(@"Using placeholder preview for file %@", path);
        }

        if (QLPreviewRequestIsCancelled(preview)) {
            return noErr;
        }

        [self finishPreviewWithPNG:preview pngPath:pngPath options:options];
        return noErr;
    }
}

- (void)finishPreviewWithPNG:(QLPreviewRequestRef)preview pngPath:(NSString *)pngPath options:(CFDictionaryRef)options
{
    CGImageRef image;
    CGFloat width, height;
    if (pngPath) {
        CGImageSourceRef source = CGImageSourceCreateWithURL((__bridge CFURLRef)SFPathToURL(pngPath), NULL);
        image = CGImageSourceCreateImageAtIndex(source, 0, NULL);
        CFRelease(source);
        width = CGImageGetWidth(image);
        height = CGImageGetHeight(image);
        DbgLog(@"width = %d, height = %d", (int)width, (int)height);
    } else {
        width = 400;
        height = 300;
    }

    NSString *displayName = (NSString *)[(__bridge NSDictionary *)options
        objectForKey:(__bridge NSString *)kQLPreviewPropertyDisplayNameKey];

    NSDictionary *newOpt = [NSDictionary
        dictionaryWithObjectsAndKeys:displayName,
                                    kQLPreviewPropertyDisplayNameKey,
                                    [NSNumber numberWithFloat:width],
                                    kQLPreviewPropertyWidthKey,
                                    [NSNumber numberWithFloat:height],
                                    kQLPreviewPropertyHeightKey,
                                    @"image/png",
                                    kQLPreviewPropertyMIMETypeKey,
                                    nil];

    CGContextRef ctx =
        QLPreviewRequestCreateContext(preview,
                                      CGSizeMake(width, height),
                                      YES,
                                      (__bridge CFDictionaryRef)newOpt);

    if (pngPath) {
        CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), image);
        CGImageRelease(image);
    }
    QLPreviewRequestFlushContext(preview, ctx);
    CGContextRelease(ctx);
}

- (BOOL)isFileCached:(NSString *)path
{
    BOOL cached = NO;
    if (![self.client isFileCached:path output:&cached]) {
        return NO;
    }
    DbgLog(@"file %@ is %s", path, cached? "cached" : "not cached");
    return cached;
}

- (BOOL)askForThumbnail:(NSString *)path size:(CGFloat)size output:(NSString **)output
{
    return [self.client askForThumbnail:path size:size output:output];
}
@end


extern "C" {

id<SFQLGen> getDefaultSFQLGen() {
    return [SFQLGenImpl sharedInstance];
}

}
