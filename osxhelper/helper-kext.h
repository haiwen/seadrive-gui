#import <Foundation/Foundation.h>

#import "helper-defines.h"


@interface HelperKext : NSObject

/*!
 Install.
 */
+ (void)installWithSource:(NSString *)source destination:(NSString *)destination kextID:(NSString *)kextID kextPath:(NSString *)kextPath completion:(HelperOnCompletion)completion;

/*!
 Uninstall.
 */
+ (void)uninstallWithDestination:(NSString *)destination kextID:(NSString *)kextID completion:(HelperOnCompletion)completion;

/*!
 Copy to destination.
 */
+ (void)copyWithSource:(NSString *)source destination:(NSString *)destination removeExisting:(BOOL)removeExisting completion:(HelperOnCompletion)completion;

/*!
 Always loads the kext (no-op if it is already loaded).
 */
+ (void)loadKextID:(NSString *)kextID path:(NSString *)path completion:(HelperOnCompletion)completion;

+ (void)unloadKextID:(NSString *)kextID completion:(HelperOnCompletion)completion;


+ (BOOL)updateLoaderFileAttributes:(NSString *)destination error:(NSError **)error;

@end
