#import "helper-kext.h"

#import <IOKit/kext/KextManager.h>
#import <grp.h>
#import <sys/stat.h>
#import <sys/sysctl.h>
#import <sys/types.h>
#import <uuid/uuid.h>
#import "helper-log.h"

#define MACOSX_ADMIN_GROUP_NAME "admin"
#define SEADRIVEFS_SYSCTL_TUNABLES_ADMIN "vfs.generic.seadrivefs.tunables.admin_group"

@implementation HelperKext

+ (NSDictionary *)kextInfo:(NSString *)label {
  NSDictionary *kexts = (__bridge NSDictionary *)KextManagerCopyLoadedKextInfo((__bridge CFArrayRef)@[label], NULL);
  return kexts[label];
}

+ (BOOL)isKextLoaded:(NSString *)label {
  NSDictionary *kexts = (__bridge NSDictionary *)KextManagerCopyLoadedKextInfo((__bridge CFArrayRef)@[label], (__bridge CFArrayRef)@[@"OSBundleStarted"]);
  return [kexts[label][@"OSBundleStarted"] boolValue];
}

+ (void)installWithSource:(NSString *)source destination:(NSString *)destination kextID:(NSString *)kextID kextPath:(NSString *)kextPath completion:(HelperOnCompletion)completion {
  NSError *error = nil;

  // Uninstall if installed
  if (![self uninstallWithDestination:destination kextID:kextID error:&error]) {
    if (!error) error = HelperMakeError(HelperErrorKext, @"Failed to uninstall");
    completion(error, @(0));
    return;
  }

  // Copy kext into place
  [self copyWithSource:source destination:destination removeExisting:NO completion:^(NSError *error, id value) {
    if (error) {
      completion(error, nil);
      return;
    }
    [self loadKextID:kextID path:kextPath completion:completion];

    // Make any user in the admin group also admins of the seadrivefs
    // admin group. Otherwise we can not use "allow_other" flag when
    // mounting.
    // See https://github.com/osxfuse/osxfuse/wiki/Mount-options#allow_other
    struct group *admin_group = getgrnam(MACOSX_ADMIN_GROUP_NAME);
    if (admin_group) {
        int admin_gid = admin_group->gr_gid;
        HelperLog(@"setting seadrivefs admin group to osx admin group (group id = %d)", admin_group);
        (void)sysctlbyname(SEADRIVEFS_SYSCTL_TUNABLES_ADMIN, NULL, NULL,
                           &admin_gid, sizeof(admin_gid));
    } else {
        HelperLog(@"seadrivefs admin group not set because reading admin group failed");
    }
  }];
}

+ (void)copyWithSource:(NSString *)source destination:(NSString *)destination removeExisting:(BOOL)removeExisting completion:(HelperOnCompletion)completion {
  NSError *error = nil;

  if (removeExisting && ![self deletePath:destination error:&error]) {
    if (!error) error = HelperMakeError(HelperErrorKext, @"Failed to remove existing");
    completion(error, nil);
    return;
  }

  if (![NSFileManager.defaultManager copyItemAtPath:source toPath:destination error:&error]) {
    if (!error) error = HelperMakeError(HelperErrorKext, @"Failed to copy");
    completion(error, nil);
    return;
  }

  [self updateAttributes:0 gid:0 perm:0755 path:destination completion:^(NSError *error) {
    if (error) {
      completion(error, nil);
      return;
    }
    // if (![self updateLoaderFileAttributes:destination error:&error]) {
    //   completion(error, nil);
    //   return;
    // }
    completion(nil, nil);
  }];
}

+ (NSNumber *)permissionsForPath:(NSString *)path {
  NSDictionary *fileAttributes = [NSFileManager.defaultManager attributesOfItemAtPath:path error:nil];
  if (!fileAttributes) return nil;
  return fileAttributes[NSFilePosixPermissions];
}

+ (BOOL)updateLoaderFileAttributes:(NSString *)destination error:(NSError **)error {
  NSString *path = [NSString stringWithFormat:@"%@/Contents/Resources/load_macfuse", destination];
  return [self setUID:path error:error];
}

+ (BOOL)setUID:(NSString *)path error:(NSError **)error {
  mode_t perm = 04755;
  const char *file = [NSFileManager.defaultManager fileSystemRepresentationWithPath:path];
  int err = chmod(file, perm);
  if (err != 0) {
    if (error) *error = HelperMakeError(HelperErrorKext, @"Unable to set permissions for %@; chown error: %@", path, @(err));
    return NO;
  }
  HelperLog(@"Permissions for %@: %o", path, [[self permissionsForPath:path] shortValue]);
  return YES;
}

+ (void)updateAttributes:(uid_t)uid gid:(gid_t)gid perm:(mode_t)perm path:(NSString *)path completion:(HelperCompletion)completion {
  NSError *error = nil;
  if (![self updateAttributes:uid gid:gid perm:perm path:path error:&error]) {
    if (!error) error = HelperMakeError(HelperErrorKext, @"Failed to set attributes");
    completion(error);
    return;
  }

  NSDirectoryEnumerator *enumerator = [NSFileManager.defaultManager enumeratorAtPath:path];
  NSString *file;
  while ((file = [enumerator nextObject])) {
    if (![self updateAttributes:uid gid:gid perm:perm path:[path stringByAppendingPathComponent:file] error:&error]) {
      if (!error) error = HelperMakeError(HelperErrorKext, @"Failed to set attributes");
      completion(error);
      return;
    }
  }

  completion(nil);
}

+ (BOOL)updateAttributes:(uid_t)uid gid:(gid_t)gid perm:(mode_t)perm path:(NSString *)path error:(NSError **)error {
  NSDictionary *existingAttributes = [NSFileManager.defaultManager attributesOfItemAtPath:path error:error];
  if (!existingAttributes) {
    return NO;
  }
  const char *file = [NSFileManager.defaultManager fileSystemRepresentationWithPath:path];

  int chownErr = 0;
  if (existingAttributes[NSFileType] == NSFileTypeSymbolicLink) {
    chownErr = lchown(file, uid, gid);
  } else {
    chownErr = chown(file, uid, gid);
  }
  if (chownErr != 0) {
    if (error) *error = HelperMakeError(HelperErrorKext, @"Unable to chown: %@", @(chownErr));
    return NO;
  }

  int chmodErr = 0;
  chmodErr = chmod(file, perm);
  if (chmodErr != 0) {
    if (error) *error = HelperMakeError(HelperErrorKext, @"Unable to chmodErr: %@", @(chmodErr));
    return NO;
  }

  return YES;
}

+ (void)updateWithSource:(NSString *)source destination:(NSString *)destination kextID:(NSString *)kextID kextPath:(NSString *)kextPath completion:(HelperOnCompletion)completion {
  [self uninstallWithDestination:destination kextID:kextID completion:^(NSError *error, id value) {
    if (error) {
      completion(error, @(0));
      return;
    }
    [self installWithSource:source destination:destination kextID:kextID kextPath:kextPath completion:completion];
  }];
}

+ (void)loadKextID:(NSString *)kextID path:(NSString *)path completion:(HelperOnCompletion)completion {
  HelperLog(@"Loading kext path: %@", path);
  CFURLRef km_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault,
                                                   (__bridge CFStringRef)(path),
                                                   kCFURLPOSIXPathStyle,
                                                   true);
  OSReturn status = KextManagerLoadKextWithURL(km_url, NULL);
  CFRelease(km_url);

  // OSReturn status = KextManagerLoadKextWithIdentifier((__bridge CFStringRef)(kextID), (__bridge CFArrayRef)@[[NSURL fileURLWithPath:path]]);
  if (status != kOSReturnSuccess) {
    NSError *error = HelperMakeError(HelperErrorKext, @"KextManager failed to load with status: %@", @(status));
    completion(error, nil);
  } else {
    completion(nil, nil);
  }
}

+ (void)unloadKextID:(NSString *)kextID completion:(HelperOnCompletion)completion {
  NSParameterAssert(kextID);
  HelperLog(@"Unload kextID: %@ (%@)", kextID);
  NSError *error = nil;
  [self unloadKextID:kextID error:&error];
  completion(error, nil);
}

+ (BOOL)unloadKextID:(NSString *)kextID error:(NSError **)error {
  BOOL isKextLoaded = [self isKextLoaded:kextID];
  HelperLog(@"Kext loaded? %@", @(isKextLoaded));
  if (!isKextLoaded) return YES;

  return [self _unloadKextID:kextID error:error];
}

+ (BOOL)_unloadKextID:(NSString *)kextID error:(NSError **)error {
  HelperLog(@"Unloading kextID: %@", kextID);
  OSReturn status = KextManagerUnloadKextWithIdentifier((__bridge CFStringRef)kextID);
  HelperLog(@"Unload kext status: %@", @(status));
  if (status != kOSReturnSuccess) {
    if (error) *error = HelperMakeError(HelperErrorKext, @"KextManager failed to unload with status: %@: %@", @(status), [HelperKext descriptionForStatus:status]);
    return NO;
  }
  return YES;
}

+ (void)uninstallWithDestination:(NSString *)destination kextID:(NSString *)kextID completion:(HelperOnCompletion)completion {
  NSError *error = nil;
  if (![self uninstallWithDestination:destination kextID:kextID error:&error]) {
    completion(error, @(0));
    return;
  }
  completion(nil, @(0));
}

+ (BOOL)deletePath:(NSString *)path error:(NSError **)error {
  if ([NSFileManager.defaultManager fileExistsAtPath:path isDirectory:NULL] && ![NSFileManager.defaultManager removeItemAtPath:path error:error]) {
    if (error) *error = HelperMakeError(HelperErrorKext, @"Failed to remove path: %@", path);
    return NO;
  }
  return YES;
}

+ (BOOL)uninstallWithDestination:(NSString *)destination kextID:(NSString *)kextID error:(NSError **)error {
  if (![self unloadKextID:kextID error:error]) {
    return NO;
  }

  if (![self deletePath:destination error:error]) {
    return NO;
  }

  return YES;
}

+ (NSString *)descriptionForStatus:(OSReturn)status {
  switch (status) {
    case kOSMetaClassDuplicateClass:
      return @"A duplicate Libkern C++ classname was encountered during kext loading.";
    case kOSMetaClassHasInstances:
      return @"A kext cannot be unloaded because there are instances derived from Libkern C++ classes that it defines.";
    case kOSMetaClassInstNoSuper:
      return @"Internal error: No superclass can be found when constructing an instance of a Libkern C++ class.";
    case kOSMetaClassInternal:
      return @"Internal OSMetaClass run-time error.";
    case kOSMetaClassNoDicts:
      return @"Internal error: An allocation failure occurred registering Libkern C++ classes during kext loading.";
    case kOSMetaClassNoInit:
      return @"Internal error: The Libkern C++ class registration system was not properly initialized during kext loading.";
    case kOSMetaClassNoInsKModSet:
      return @"Internal error: An error occurred registering a specific Libkern C++ class during kext loading.";
    case kOSMetaClassNoKext:
      return @"Internal error: The kext for a Libkern C++ class can't be found during kext loading.";
    case kOSMetaClassNoKModSet:
      return @"Internal error: An allocation failure occurred registering Libkern C++ classes during kext loading.";
    case kOSMetaClassNoSuper:
      return @"Internal error: No superclass can be found for a specific Libkern C++ class during kext loading.";
    case kOSMetaClassNoTempData:
      return @"Internal error: An allocation failure occurred registering Libkern C++ classes during kext loading.";
    case kOSReturnError:
      return @"Unspecified Libkern error. Not equal to KERN_FAILURE.";
    case kOSReturnSuccess:
      return @"Operation successful. Equal to KERN_SUCCESS.";
    case -603947004:
      return @"Root privileges required.";
    case -603947002:
      return @"Kext not loaded.";
    default:
      return @"Unknown error unloading kext.";
  }
}

@end
