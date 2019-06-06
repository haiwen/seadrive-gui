#import <QuickLook/QuickLook.h>

#import "utils.h"
#import "qlgen.h"

/* -----------------------------------------------------------------------------
    Generate a thumbnail for file

   This function's job is to create thumbnail for designated file as fast as
   possible

   This function is a wrapper that is called by the quicklookd system
   process. The real logic are implemented in qlgen.mm and other
   modules.
   -----------------------------------------------------------------------------
   */

OSStatus GenerateThumbnailForURL(void *thisInterface,
                                 QLThumbnailRequestRef thumbnail,
                                 CFURLRef url,
                                 CFStringRef contentTypeUTI,
                                 CFDictionaryRef options,
                                 CGSize maxSize)
{
    DbgLog(@"Generating %dx%d Thumbnail for %@",
           (int)maxSize.width,
           (int)maxSize.height,
           SFURLToPath((__bridge NSURL *)url));
    if (QLThumbnailRequestIsCancelled(thumbnail))
        return noErr;

    id<SFQLGen> qlgen = getDefaultSFQLGen();
    return [qlgen genThumnail:thumbnail
                          url:url
               contentTypeUTI:contentTypeUTI
                      options:options
                      maxSize:maxSize];
}

void CancelThumbnailGeneration(void *thisInterface,
                               QLThumbnailRequestRef thumbnail)
{
    // implement only if supported
}
