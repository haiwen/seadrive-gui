#import <Cocoa/Cocoa.h>

#import "utils.h"
#import "qlgen-client.h"
#import "system-qlgen.h"
#import "qlgen.h"


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
    if (!UTTypeConformsTo(contentTypeUTI,
                         (__bridge CFStringRef)@"public.image")) {
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
            // not runnin. In such case we treat all files as local
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
    BOOL localOrCached = [self isFileLocalOrCached:path];
    if (!localOrCached) {
        return noErr;
    }

    if (QLPreviewRequestIsCancelled(preview)) {
        return noErr;
    }

    DbgLog(@"calling system qlgen for file %@", path);
    SystemQLGen *system = [SystemQLGen sharedInstance];
    return [system genPreview:preview
                          url:url
               contentTypeUTI:contentTypeUTI
                      options:options];
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
