//
//  FinderSync.h
//  seadrive-fsplugin
//
//  Created by Chilledheart on 1/10/15.
//  Copyright (c) 2015 Haiwen. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <FinderSync/FinderSync.h>

#include <string>

@interface FinderSync : FIFinderSync
- (void)updateWatchSet:(void *)ptr_to_new_repos;
- (void)updateFileStatus:(const char *)path
                  status:(uint32_t)status;
@end

#define toNSString(x) [NSString stringWithUTF8String:(x).c_str()]
#define toStdString(x) std::string([(x) UTF8String])
