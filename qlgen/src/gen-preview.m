#import <QuickLook/QuickLook.h>
#import "utils.h"
#import "qlgen.h"

/* -----------------------------------------------------------------------------
 Generate a preview for file

 This function's job is to create preview for designated file
 -----------------------------------------------------------------------------
 */

OSStatus GeneratePreviewForURL(void *thisInterface,
                               QLPreviewRequestRef preview,
                               CFURLRef url,
                               CFStringRef contentTypeUTI,
                               CFDictionaryRef options)
{
    DbgLog(@"Generating preview for %@", SFURLToPath((__bridge NSURL *)url));
    if (QLPreviewRequestIsCancelled(preview))
        return noErr;
    id<SFQLGen> qlgen = getDefaultSFQLGen();
    return [qlgen genPreview:preview
                         url:url
              contentTypeUTI:contentTypeUTI
                     options:options];
}

void CancelPreviewGeneration(void *thisInterface, QLPreviewRequestRef preview)
{
    // implement only if supported
}
