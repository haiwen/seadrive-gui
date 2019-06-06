#import <Cocoa/Cocoa.h>
#import <CoreFoundation/CFPlugInCOM.h>

#import "utils.h"
#import "system-qlgen.h"

@interface SysPlugin ()
@property (copy) NSString *identifier;
@property CFPlugInRef plugin;
@property QLGeneratorInterfaceStruct **interface;
- (void) dealloc;
@end

@implementation SysPlugin
- (void)dealloc {
    DbgLog(@"SysPlugin::dealloc is called");
    if (self.interface) {
        (*(self.interface))->Release(self.interface);
    }
    if (self.plugin) {
        CFRelease(self.plugin);
    }
}
@end

extern "C" {

@interface SystemQLGen ()
// map of UTI => system generator
@property NSMutableDictionary *sysGenenetors;
-(void)loadGeneratorsFromOneSysDir:(NSString *)sysdir;
-(BOOL)loadOnePlugin:(NSString *)path;
-(NSArray *)extractUTIs:(NSDictionary *)infoPlist;
- (void) dealloc;
@end

@implementation SystemQLGen
+ (instancetype)sharedInstance
{
    static SystemQLGen *instance = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
      instance = [SystemQLGen new];
    });
    return instance;
}

- (OSStatus)genThumnail:(QLThumbnailRequestRef)thumbnail
                    url:(CFURLRef)url
         contentTypeUTI:(CFStringRef)_contentTypeUTI
                options:(CFDictionaryRef)options
                maxSize:(CGSize)maxSize
{
    NSString *contentTypeUTI = (__bridge NSString*)_contentTypeUTI;
    [self loadAllSystemGenerators];
    SysPlugin *matchedPlugin = [self findPluginForUTI:contentTypeUTI];
    if (!matchedPlugin) {
        NSLog(@"No sys plugin found for uti %@", contentTypeUTI);
        return noErr;
    }
    QLGeneratorInterfaceStruct **interface = matchedPlugin.interface;
    return (*interface)
                ->GenerateThumbnailForURL((*interface),
                                        thumbnail,
                                        url,
                                        _contentTypeUTI,
                                        options,
                                        maxSize);
}

- (OSStatus)genPreview:(QLPreviewRequestRef)preview
                    url:(CFURLRef)url
         contentTypeUTI:(CFStringRef)_contentTypeUTI
                options:(CFDictionaryRef)options
{
    NSString *contentTypeUTI = (__bridge NSString*)_contentTypeUTI;
    [self loadAllSystemGenerators];
    SysPlugin *matchedPlugin = [self findPluginForUTI:contentTypeUTI];
    if (!matchedPlugin) {
        NSLog(@"No sys plugin found for uti %@", contentTypeUTI);
        return noErr;
    }
    QLGeneratorInterfaceStruct **interface = matchedPlugin.interface;
    return (*interface)
                ->GeneratePreviewForURL((*interface),
                                        preview,
                                        url,
                                        _contentTypeUTI,
                                        options);
}


// 1. Enumberate all subdirs of system quicklook generators location
// 2. For each generator, parse its Info.plist to get a list of supported UTIs
// 3. Insert the (UTI, plugin) pair into the dictionary
-(BOOL)loadAllSystemGenerators {
    if (self.sysGenenetors) {
        return YES;
    }

    self.sysGenenetors = [NSMutableDictionary new];
    // TODO: also load user-installed generators, located at
    // ~/Library/Quicklook (but excluding this generator itself).
    NSArray *sysdirs = [NSArray arrayWithObjects:@"/Library/QuickLook",
                                                 @"/System/Library/QuickLook",
                                                 nil];

    [sysdirs enumerateObjectsUsingBlock:^(
                 NSString *sysdir, NSUInteger idx, BOOL *stop) {
      [self loadGeneratorsFromOneSysDir:sysdir];
    }];

    return YES;
}

- (void)loadGeneratorsFromOneSysDir:(NSString *)sysdir {
    DbgLog(@"loading system generators in dir %@", sysdir);
    NSDirectoryEnumerator *subdirsEnum = [[NSFileManager defaultManager]
                   enumeratorAtURL:SFPathToURL(sysdir)
        includingPropertiesForKeys:[NSArray
                                       arrayWithObjects:NSURLIsDirectoryKey,
                                                        nil]
                           options:
                               NSDirectoryEnumerationSkipsSubdirectoryDescendants
                      errorHandler:nil];
    NSURL *url;
    while ((url = [subdirsEnum nextObject])) {
        NSString *path = SFURLToPath(url);
        NSNumber *isDir;
        [url getResourceValue:&isDir forKey:NSURLIsDirectoryKey error:nil];
        if (![isDir boolValue]) {
            DbgLog(@"ignore non-subdir %@", path);
            continue;
        }
        if (![self loadOnePlugin:path]) {
            NSLog(@"failed to load system plugin %@", path);
        } else {
            DbgLog(@"Loaded system plugin from %@", path);
        }
    }
}

-(BOOL)loadOnePlugin:(NSString *)path {
    QLGeneratorInterfaceStruct **interface = NULL;
    CFURLRef plugin_url = CFURLCreateWithFileSystemPath(
        NULL,
        (__bridge CFStringRef)path,
        kCFURLPOSIXPathStyle,
        TRUE);

    CFPlugInRef plugin = CFPlugInCreate(NULL, plugin_url);
    if (!plugin) {
        NSLog(@"Could not create CFPluginRef");
        return NO;
    }

    CFArrayRef factories = CFPlugInFindFactoriesForPlugInTypeInPlugIn(kQLGeneratorTypeID, plugin);
    if (!factories || !CFArrayGetCount(factories)) {
        NSLog(@"Could not find any factories");
        CFRelease(factories);
        return NO;
    }

    CFUUIDRef factoryID = (CFUUIDRef) CFArrayGetValueAtIndex(factories, 0);
    CFRelease(factories);
    IUnknownVTbl **iunknown =
        (IUnknownVTbl **)CFPlugInInstanceCreate(NULL, factoryID, kQLGeneratorTypeID);
    if (!iunknown) {
        NSLog(@"Failed to create instance");
        return NO;
    }

    (*iunknown)->QueryInterface(
        iunknown,
        CFUUIDGetUUIDBytes(kQLGeneratorCallbacksInterfaceID),
        (LPVOID *)(&interface));

    // Now we are done with IUnknown
    (*iunknown)->Release(iunknown);

    if (!interface) {
        NSLog(@"Failed to get interface");
        return NO;
    }

    SysPlugin *sysPlugin = [SysPlugin new];
    sysPlugin.interface = interface;
    sysPlugin.plugin = plugin;

    // This dictionary is owned by the plugin so we don't need to free
    // it on our side
    NSDictionary *infoPlist = (__bridge NSDictionary*)CFBundleGetInfoDictionary(plugin);
    if (!infoPlist) {
        NSLog(@"Fail to load info.plist for %@", path);
        return NO;
    }
    sysPlugin.identifier = [infoPlist objectForKey:@"CFBundleIdentifier"];
    // DbgLog(@"info.plist = %@", infoPlist);

    NSArray *contentTypeUTIs = [self extractUTIs:infoPlist];
    if (!contentTypeUTIs || [contentTypeUTIs count] == 0) {
        NSLog(@"No UTI found in Info.plist for %@", path);
        return NO;
    }
    [contentTypeUTIs enumerateObjectsUsingBlock:^(
                         NSString *uti, NSUInteger idx, BOOL *stop) {
      [self.sysGenenetors setObject:sysPlugin forKey:uti];
      // DbgLog(@"Using %@ for UTI %@", path, uti);
    }];

    return YES;
}

// Note: we can't use objc array/dict literals (i.e. a[0], d[@"x"])
// because they are not supported on OSX 10.7
-(NSArray *)extractUTIs:(NSDictionary *)infoPlist {
    NSArray *documentTypes = [infoPlist objectForKey:@"CFBundleDocumentTypes"];
    if (!documentTypes) {
        NSLog(@"CFBundleDocumentTypes not found");
        return nil;
    }
    for (NSUInteger i = 0; i < [documentTypes count]; i++) {
        NSDictionary *typeDict = [documentTypes objectAtIndex:i];
        NSString *typeRole = [typeDict objectForKey:@"CFBundleTypeRole"];
        if (typeRole && [typeRole isEqualToString:@"QLGenerator"]) {
            return [typeDict objectForKey:@"LSItemContentTypes"];
        }
    }
    return nil;
}

- (SysPlugin *)findPluginForUTI:(NSString *)currentUTI
{
    // TODO: cache the result instead of iterating through the dict on
    // each call.

    // First try to find an exact match. We can't skip this because
    // otherwise a conform-to match may be chosen even if there is an
    // exact match.
    SysPlugin *plugin = [self.sysGenenetors objectForKey:currentUTI];
    if (plugin) {
        DbgLog(@"Found exact match for UTI %@", currentUTI);
        return plugin;
    }

    // Then try to find an conform-to match
    //
    // TODO: For now we use the first conform-to match found during
    // the iteration. However, if there are multiple matches, shall we
    // use the most specific one? E.g. between "public.image" and
    // "public.jpeg" we should choose the latter.
    __block NSString *matchedUTI;
    [self.sysGenenetors enumerateKeysAndObjectsUsingBlock:^(
                            NSString *key, id value, BOOL *stop) {
      if (UTTypeConformsTo((__bridge CFStringRef)currentUTI,
                           (__bridge CFStringRef)key)) {
          matchedUTI = key;
          *stop = YES;
      }
    }];

    if (matchedUTI) {
        DbgLog(@"Using parent UTI %@ for %@", matchedUTI, currentUTI);
        return [self.sysGenenetors objectForKey:matchedUTI];
    }
    return nil;
}

- (void)dealloc {
    DbgLog(@"SystemQLGen::dealloc is called");
}
@end

} // extern "C"
