//
//  FinderSync.m
//  seadrive-fsplugin
//
//  Created by Chilledheart on 1/10/15.
//  Copyright (c) 2015 Haiwen. All rights reserved.
//

#import "FinderSync.h"
#import "FinderSyncClient.h"

#include <utility>
#include <map>
#include <unordered_map>
#include <algorithm>

#if !__has_feature(objc_arc)
#error this file must be built with ARC support
#endif

#if defined(NDEBUG)
// Make the debug log a no-op in non-debug mode
#define SEAFILE_DEBUG_LOG(args, ...)
#else
#define SEAFILE_DEBUG_LOG NSLog
#endif

static std::vector<std::string> watched_repos_;
static std::string mount_point_;
static std::unordered_map<std::string, PathStatus> file_status_;
static FinderSyncClient *client_ = nullptr;
static constexpr double kGetWatchSetInterval = 5.0;   // seconds
static constexpr double kGetFileStatusInterval = 2.0; // seconds

@interface FinderSync ()

@property(readwrite, nonatomic, strong) NSTimer *update_watch_set_timer_;
@property(readwrite, nonatomic, strong) NSTimer *update_file_status_timer_;
@property(readwrite, nonatomic, strong) dispatch_queue_t client_command_queue_;
@end

static const char *const kClientCommandQueueName =
    "com.seafile.seadrive.findersync.ClientCommandQueue";
static const NSArray *const kBadgetIdentifiers = @[
    // According to the document
    // https://developer.apple.com/library/mac/documentation/FinderSync/Reference/FIFinderSyncController_Class/#//apple_ref/occ/instm/FIFinderSyncController/setBadgeIdentifier:forURL:
    // Setting the identifier to an empty string (@"") removes the badge.
    @"",                        // none
    @"syncing",                 // syncing
    @"error",                   // error
    @"synced",                  // synced
    @"partial_synced",          // partial synced
    @"cloud",                   // cloud
    @"readonly",                // read-only
    @"locked",                  // locked
    @"locked_by_me",            // locked by me
];

// Set up images for our badge identifiers. For demonstration purposes,
static void initializeBadgeImages() {
    // Set up images for our badge identifiers.
    // SYNCING,
    [[FIFinderSyncController defaultController]
             setBadgeImage:[NSImage imageNamed:@"status-syncing.icns"]
                     label:NSLocalizedString(@"Syncing", @"Status Syncing")
        forBadgeIdentifier:kBadgetIdentifiers[PathStatus::SYNC_STATUS_SYNCING]];
    // ERROR,
    [[FIFinderSyncController defaultController]
             setBadgeImage:[NSImage imageNamed:@"status-error.icns"]
                     label:NSLocalizedString(@"Error", @"Status Erorr")
        forBadgeIdentifier:kBadgetIdentifiers[PathStatus::SYNC_STATUS_ERROR]];
    // SYNCED,
    [[FIFinderSyncController defaultController]
             setBadgeImage:[NSImage imageNamed:@"status-synced.icns"]
                     label:NSLocalizedString(@"Finished", @"Status Finished")
        forBadgeIdentifier:kBadgetIdentifiers[PathStatus::SYNC_STATUS_SYNCED]];

    // Partial SYNCED,
    [[FIFinderSyncController defaultController]
             setBadgeImage:[NSImage imageNamed:@"status-partial-synced.icns"]
                     label:NSLocalizedString(@"Partial Synced", @"Status Partial Finished")
        forBadgeIdentifier:kBadgetIdentifiers[PathStatus::SYNC_STATUS_PARTIAL_SYNCED]];

    // Cloud,
    [[FIFinderSyncController defaultController]
             setBadgeImage:[NSImage imageNamed:@"status-cloud.icns"]
                     label:NSLocalizedString(@"Cloud", @"Status Cloud")
        forBadgeIdentifier:kBadgetIdentifiers[PathStatus::SYNC_STATUS_CLOUD]];

    // READONLY
    [[FIFinderSyncController defaultController]
             setBadgeImage:[NSImage imageNamed:@"status-readonly.icns"]
                     label:NSLocalizedString(@"ReadOnly", @"Status ReadOnly")
        forBadgeIdentifier:kBadgetIdentifiers[PathStatus::SYNC_STATUS_READONLY]];

    // LOCKED,
    [[FIFinderSyncController defaultController]
             setBadgeImage:[NSImage imageNamed:@"status-locked.icns"]
                     label:NSLocalizedString(@"Locked", @"Status Locked")
        forBadgeIdentifier:kBadgetIdentifiers[PathStatus::SYNC_STATUS_LOCKED]];
    // LOCKED_BY_ME,
    [[FIFinderSyncController defaultController]
             setBadgeImage:[NSImage imageNamed:@"status-locked-by-me.icns"]
                     label:NSLocalizedString(@"Locked", @"Status LockedByMe")
        forBadgeIdentifier:kBadgetIdentifiers
                               [PathStatus::SYNC_STATUS_LOCKED_BY_ME]];
}

inline static void setBadgeIdentifierFor(NSURL *url, PathStatus status) {
    [[FIFinderSyncController defaultController]
        setBadgeIdentifier:kBadgetIdentifiers[status]
                    forURL:url];
}

inline static std::vector<std::string>::const_iterator
findRepo(const std::vector<std::string> &repos, const std::string &subdir) {
    auto pos = repos.begin();
    for (; pos != repos.end(); ++pos) {
        if (*pos == subdir)
            break;
    }
    return pos;
}


inline static void setBadgeIdentifierFor(const std::string &path,
                                         PathStatus status) {
    // if (findRepo(watched_repos_, path) != watched_repos_.end()) {
    //     // No icon for repo top dir.
    //     return;
    // }
    bool isDirectory = path.back() == '/';
    std::string file = path;
    if (isDirectory)
        file.resize(file.size() - 1);

    setBadgeIdentifierFor(
        [NSURL fileURLWithPath:[NSString stringWithUTF8String:file.c_str()]
                   isDirectory:isDirectory],
        status);
}

inline static bool isUnderFolderDirectly(const std::string &path,
                                         const std::string &dir) {
    if (strncmp(dir.data(), path.data(), dir.size()) != 0) {
        return false;
    }
    const char *pos = path.data() + dir.size() + 1;
    const char *end = pos + path.size() - (dir.size());
    if (end == pos)
        return true;
    // remove the trailing "/" in the end
    if (*(end - 1) == '/')
        --end;
    while (pos != end)
        if (*pos++ == '/')
            return false;
    return true;
}

inline static std::string getRelativePath(const std::string &path,
                                     const std::string &prefix) {

    std::string relative_path;
    // remove the trailing "/" in the header
    if (path.size() != prefix.size()) {
        relative_path = std::string(path.data() + prefix.size() + 1,
                                    path.size() - prefix.size() - 1);
    }
    return relative_path;
}

inline static bool isContainsPrefix(const std::string &path,
                                    const std::string &prefix) {
    if (prefix.size() > path.size())
        return false;
    if (0 != strncmp(prefix.data(), path.data(), prefix.size()))
        return false;
    if (prefix.size() < path.size() && path[prefix.size()] != '/')
        return false;
    return true;
}

inline static std::vector<std::string>::const_iterator
findRepoContainPath(const std::vector<std::string> &repos,
                    const std::string &path) {
    for (auto repo = repos.begin(); repo != repos.end(); ++repo) {
        if (isContainsPrefix(path, *repo))
            return repo;
    }
    return repos.end();
}

static bool isCategoryDir(const std::string path)
{
    return findRepo(watched_repos_, path) != watched_repos_.end();
}

inline static void cleanEntireDirectoryStatus(
    std::unordered_map<std::string, PathStatus> *file_status,
    const std::string &dir) {
    for (auto file = file_status->begin(); file != file_status->end();) {
        auto pos = file++;
        if (!isContainsPrefix(pos->first, dir))
            continue;
        setBadgeIdentifierFor(pos->first, PathStatus::SYNC_STATUS_NONE);
        file_status->erase(pos);
    }
}

inline static void
cleanDirectoryStatus(std::unordered_map<std::string, PathStatus> *file_status,
                     const std::string &dir) {
    for (auto file = file_status->begin(); file != file_status->end();) {
        auto pos = file++;
        if (!isUnderFolderDirectly(pos->first, dir))
            continue;
        file_status->erase(pos);
    }
}

static void
cleanFileStatus(std::unordered_map<std::string, PathStatus> *file_status,
                const std::vector<std::string> &watch_set,
                const std::vector<std::string> &new_watch_set) {
    for (const auto &repo : watch_set) {
        bool found = false;
        for (const auto &new_repo : new_watch_set) {
            if (repo == new_repo) {
                found = true;
                break;
            }
        }
        // cleanup old
        if (!found) {
            // clean up leafs
            cleanEntireDirectoryStatus(file_status, repo);
        }
    }
}

@implementation FinderSync

- (instancetype)init {
    self = [super init];

#ifdef NDEBUG
    NSLog(@"%s launched from %@ ; compiled at %s", __PRETTY_FUNCTION__,
          [[NSBundle mainBundle] bundlePath], __DATE__);
#else
    NSLog(@"%s launched from %@ ; compiled at %s %s", __PRETTY_FUNCTION__,
          [[NSBundle mainBundle] bundlePath], __TIME__, __DATE__);
#endif

    // Set up client queue
    self.client_command_queue_ =
        dispatch_queue_create(kClientCommandQueueName, DISPATCH_QUEUE_SERIAL);
    // Set up client
    client_ = new FinderSyncClient(self);
    self.update_watch_set_timer_ =
        [NSTimer scheduledTimerWithTimeInterval:kGetWatchSetInterval
                                         target:self
                                       selector:@selector(requestUpdateWatchSet)
                                       userInfo:nil
                                        repeats:YES];

    self.update_file_status_timer_ = [NSTimer
        scheduledTimerWithTimeInterval:kGetFileStatusInterval
                                target:self
                              selector:@selector(requestUpdateFileStatus)
                              userInfo:nil
                               repeats:YES];

    // NSMutableArray *array = [NSMutableArray arrayWithCapacity:1];
    // NSString *path = [NSString stringWithUTF8String:"/Users/lin/SeaDrive/My Library"];
    // [array addObject:[NSURL fileURLWithPath:path isDirectory:YES]];
    // [FIFinderSyncController defaultController].directoryURLs = [NSSet setWithArray:array];

    [FIFinderSyncController defaultController].directoryURLs = nil;

    return self;
}

- (void)dealloc {
    delete client_;
    self.client_command_queue_ = nil;
}

#pragma mark - Primary Finder Sync protocol methods

- (void)beginObservingDirectoryAtURL:(NSURL *)url {
    // convert NFD to NFC
    std::string absolute_path =
        url.path.precomposedStringWithCanonicalMapping.UTF8String;

    SEAFILE_DEBUG_LOG (@"FinderSync: beginObservingDirectoryAtURL called for %s", absolute_path.c_str());

    // find where we have it
    auto repo = findRepoContainPath(watched_repos_, absolute_path);
    if (repo == watched_repos_.end()) {
        return;
    }


    file_status_.emplace(absolute_path, PathStatus::SYNC_STATUS_NONE);
}

- (void)endObservingDirectoryAtURL:(NSURL *)url {
    // This check is for the situation that there are subdirs of the seadrive
    // mounted point opened in finder, then at the moment when seadrive exits,
    // this callback would be invoked, but since the NSURL is no longer valid
    // (because seadrive is unmounted), the `path` property of the url is nil.
    if (!url.path) {
        return;
    }

    // convert NFD to NFC
    std::string absolute_path =
        url.path.precomposedStringWithCanonicalMapping.UTF8String;

    SEAFILE_DEBUG_LOG (@"FinderSync: endObservingDirectoryAtURL called for %s", absolute_path.c_str());

    // if (absolute_path.back() != '/')
    //     absolute_path += "/";

    cleanDirectoryStatus(&file_status_, absolute_path);
}

- (void)requestBadgeIdentifierForURL:(NSURL *)url {
    if (!url.path) {
        return;
    }

    std::string file_path =
        url.path.precomposedStringWithCanonicalMapping.UTF8String;

    SEAFILE_DEBUG_LOG (@"FinderSync: requestBadgeIdentifierForURL called for %s", file_path.c_str());

    // if (findRepo(watched_repos_, file_path) != watched_repos_.end()) {
    //     // No icon for repo top dir.
    //     return;
    // }

    auto repo = findRepoContainPath(watched_repos_, file_path);
    if (repo == watched_repos_.end()) {
        return;
    }

    file_status_.emplace(file_path, PathStatus::SYNC_STATUS_NONE);
    setBadgeIdentifierFor(file_path, PathStatus::SYNC_STATUS_NONE);

    dispatch_async(self.client_command_queue_, ^{
            client_->doGetFileStatus(file_path.c_str());
    });
}

#pragma mark - Menu and toolbar item support

#if 0
- (NSString *)toolbarItemName {
  return @"SeaDrive FinderSync";
}

- (NSString *)toolbarItemToolTip {
  return @"SeaDrive FinderSync: Click the toolbar item for a menu.";
}

- (NSImage *)toolbarItemImage {
  return [NSImage imageNamed:NSImageNameFolder];
}
#endif

- (NSMenu *)menuForMenuKind:(FIMenuKind)whichMenu {
    if (whichMenu != FIMenuKindContextualMenuForItems &&
        whichMenu != FIMenuKindContextualMenuForContainer)
        return nil;

    SEAFILE_DEBUG_LOG (@"FinderSync: menuForMenuKind called for");

    // Produce a menu for the extension.
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@""];
    NSMenuItem *shareLinkItem =
        [menu addItemWithTitle:NSLocalizedString(@"Get Seafile Share Link",
                                                 @"Get Seafile Share Link")
                        action:@selector(shareLinkAction:)
                 keyEquivalent:@""];
    NSImage *seafileImage = [NSImage imageNamed:@"seadrive.icns"];
    [shareLinkItem setImage:seafileImage];

    NSArray *items =
        [[FIFinderSyncController defaultController] selectedItemURLs];
    if (![items count])
        return nil;
    NSURL *item = items.firstObject;
    std::string file_path =
        item.path.precomposedStringWithCanonicalMapping.UTF8String;

    if (internal_link_supported &&
        findRepoContainPath(watched_repos_, file_path) !=
            watched_repos_.end()) {
        NSMenuItem *internalLinkItem = [menu
            addItemWithTitle:NSLocalizedString(@"Get Seafile Internal Link",
                                               @"Get Seafile Internal Link")
                      action:@selector(internalLinkAction:)
               keyEquivalent:@""];
        [internalLinkItem setImage:seafileImage];
    }

    NSMenuItem *downloadFileItem =
       [menu addItemWithTitle:NSLocalizedString(@"Download",
                                                @"Download")
                       action:@selector(downloadFileAction:)
                keyEquivalent:@""];
    [downloadFileItem setImage:seafileImage];

    // add item for uncacher file or folder
    NSMenuItem *uncacheItem =
        [menu addItemWithTitle:NSLocalizedString(@"Uncache",
                                                 @"Uncache")
                        action:@selector(uncacheAction:)
                    keyEquivalent:@""];
        [uncacheItem setImage:seafileImage];

    // add a menu item for lockFile
    if (isCategoryDir(file_path)) {
        return nil;
    }

    NSNumber *isDirectory;
    bool is_dir = [item getResourceValue:&isDirectory
                                  forKey:NSURLIsDirectoryKey
                                   error:nil] &&
                  [isDirectory boolValue];

    // we don't have a lock-file menuitem for folders
    // early return
    if (is_dir)
        return menu;

    // find where we have it
    auto file = file_status_.find(is_dir ? file_path + "/" : file_path);
    bool shouldShowLockedByMenu = false;
    if (file != file_status_.end()) {
        NSString *lockFileTitle;
        switch (file->second) {
        case PathStatus::SYNC_STATUS_READONLY:
            break;
        case PathStatus::SYNC_STATUS_LOCKED_BY_ME:
            lockFileTitle =
                NSLocalizedString(@"Unlock This File", @"Unlock This File");
            break;
        case PathStatus::SYNC_STATUS_LOCKED:
            shouldShowLockedByMenu = true;
            break;
        default:
            lockFileTitle = NSLocalizedString(@"Lock This File", @"Lock This File");
        }
        if (lockFileTitle) {
            NSMenuItem *lockFileItem = [menu addItemWithTitle:lockFileTitle
                                                       action:@selector(lockFileAction:)
                                                keyEquivalent:@""];

            [lockFileItem setImage:seafileImage];
        }
    }

    if (shouldShowLockedByMenu) {
        NSMenuItem *showLockedByMenuItem =
            [menu addItemWithTitle:NSLocalizedString(@"Locked by ...",
                                                     @"Locked by ...")
                            action:@selector(showLockedByAction:)
                     keyEquivalent:@""];
        [showLockedByMenuItem setImage:seafileImage];
    }

    NSMenuItem *showHistoryItem =
        [menu addItemWithTitle:NSLocalizedString(@"View File History",
                                                 @"View File History")
                        action:@selector(showHistoryAction:)
                 keyEquivalent:@""];
    [showHistoryItem setImage:seafileImage];

    return menu;
}

- (IBAction)shareLinkAction:(id)sender {
    NSArray *items =
        [[FIFinderSyncController defaultController] selectedItemURLs];
    if (![items count])
        return;
    NSURL *item = items.firstObject;

    // do it in another thread
    std::string path =
        item.path.precomposedStringWithCanonicalMapping.UTF8String;
    NSNumber *isDirectory;
    if ([item getResourceValue:&isDirectory
                        forKey:NSURLIsDirectoryKey
                         error:nil] &&
        [isDirectory boolValue])
        path += "/";

    dispatch_async(self.client_command_queue_, ^{
      client_->doSendCommandWithPath(FinderSyncClient::DoShareLink,
                                     path.c_str());
    });
}

- (IBAction)uncacheAction:(id)sender {
    NSArray *items =
        [[FIFinderSyncController defaultController] selectedItemURLs];
    if (![items count])
        return;
    NSURL *item = items.firstObject;

    // do it in another thread
    std::string path =
        item.path.precomposedStringWithCanonicalMapping.UTF8String;
    NSNumber *isDirectory;
    if ([item getResourceValue:&isDirectory
                        forKey:NSURLIsDirectoryKey
                         error:nil] &&
        [isDirectory boolValue])
        path += "/";

    dispatch_async(self.client_command_queue_, ^{
      client_->doSendCommandWithPath(FinderSyncClient::DoUncache,
                                     path.c_str());
    });

}

- (IBAction)downloadFileAction:(id)sender {
    NSArray *items =
        [[FIFinderSyncController defaultController] selectedItemURLs];
    if (![items count])
        return;
    NSURL *item = items.firstObject;

    // do it in another thread
    std::string path =
        item.path.precomposedStringWithCanonicalMapping.UTF8String;
    NSNumber *isDirectory;
    if ([item getResourceValue:&isDirectory
                        forKey:NSURLIsDirectoryKey
                         error:nil] &&
        [isDirectory boolValue])
        path += "/";

    dispatch_async(self.client_command_queue_, ^{
      client_->doSendCommandWithPath(FinderSyncClient::DoDownloadFile,
                                     path.c_str());
    });
}

- (IBAction)internalLinkAction:(id)sender {
    NSArray *items =
        [[FIFinderSyncController defaultController] selectedItemURLs];
    if (![items count])
        return;
    NSURL *item = items.firstObject;

    // do it in another thread
    std::string path =
        item.path.precomposedStringWithCanonicalMapping.UTF8String;
    NSNumber *isDirectory;
    if ([item getResourceValue:&isDirectory
                        forKey:NSURLIsDirectoryKey
                         error:nil] &&
        [isDirectory boolValue])
        path += "/";

    dispatch_async(self.client_command_queue_, ^{
      client_->doSendCommandWithPath(FinderSyncClient::DoInternalLink,
                                     path.c_str());
    });
}

- (IBAction)lockFileAction:(id)sender {
    NSArray *items =
        [[FIFinderSyncController defaultController] selectedItemURLs];
    if (![items count])
        return;
    NSURL *item = items.firstObject;

    std::string path =
        item.path.precomposedStringWithCanonicalMapping.UTF8String;
    // find where we have it
    auto file = file_status_.find(path);
    if (file == file_status_.end())
        return;
    if (file->second == PathStatus::SYNC_STATUS_LOCKED)
        return;

    FinderSyncClient::CommandType command;
    if (file->second == PathStatus::SYNC_STATUS_LOCKED_BY_ME)
        command = FinderSyncClient::DoUnlockFile;
    else
        command = FinderSyncClient::DoLockFile;

    // we cannot lock a directory
    NSNumber *isDirectory;
    if ([item getResourceValue:&isDirectory
                        forKey:NSURLIsDirectoryKey
                         error:nil] &&
        [isDirectory boolValue])
        return;

    // do it in another thread
    dispatch_async(self.client_command_queue_, ^{
      client_->doSendCommandWithPath(command, path.c_str());
    });
}

- (IBAction)showHistoryAction:(id)sender {
    NSArray *items =
        [[FIFinderSyncController defaultController] selectedItemURLs];
    if (![items count])
        return;
    NSURL *item = items.firstObject;

    // do it in another thread
    std::string path =
        item.path.precomposedStringWithCanonicalMapping.UTF8String;
    dispatch_async(self.client_command_queue_, ^{
      client_->doSendCommandWithPath(FinderSyncClient::DoShowFileHistory,
                                     path.c_str());
    });
}


- (void)requestUpdateWatchSet {
    // do it in another thread
    dispatch_async(self.client_command_queue_, ^{
      client_->getWatchSet();
    });
}

- (void)updateWatchSet:(void *)ptr_to_new_watched_repos {
    std::vector<std::string> new_watched_repos;
    if (ptr_to_new_watched_repos)
        new_watched_repos = std::move(
            *static_cast<std::vector<std::string> *>(ptr_to_new_watched_repos));

    SEAFILE_DEBUG_LOG (@"FinderSync: get %lu repos to watch, file status map size %lu", new_watched_repos.size(), file_status_.size());
    cleanFileStatus(&file_status_, watched_repos_, new_watched_repos);

    // overwrite the old watch set
    watched_repos_ = std::move(new_watched_repos);

    // update FIFinderSyncController's directory URLs
    NSMutableArray *array =
        [NSMutableArray arrayWithCapacity:watched_repos_.size()];
    for (const std::string &repo : watched_repos_) {
        NSString *path = [NSString stringWithUTF8String:repo.c_str()];
        [array addObject:[NSURL fileURLWithPath:path isDirectory:YES]];
    }

    // Add the SeaDrive mount dir to the watched directory, otherwise the icons
    // for a repo's top-level folder won't be refreshed in-time.
    if (watched_repos_.size()) {
        NSString *repo = [NSString stringWithUTF8String:watched_repos_[0].c_str()];
        NSString *mount_dir = [repo stringByDeletingLastPathComponent];
        [array addObject:[NSURL fileURLWithPath:mount_dir isDirectory:YES]];
    }

    [FIFinderSyncController defaultController].directoryURLs =
        [NSSet setWithArray:array];

    // initialize the badge images
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        initializeBadgeImages();
    }
}

- (void)requestUpdateFileStatus {
    for (const auto &pair : file_status_) {
        auto repo = findRepoContainPath(watched_repos_, pair.first);
        if (repo == watched_repos_.end())
            continue;

        // Capture the current value of the path so we can use it in the blocks
        // safely.
        std::string path_in_block = pair.first;

        dispatch_async(self.client_command_queue_, ^{
                client_->doGetFileStatus(path_in_block.c_str());
        });
    }
}

- (void)updateFileStatus:(const char*)path
                  status:(uint32_t)status {
    // Ignore the update if if the path is not monitored anymore.
    auto file = file_status_.find(path);
    if (file == file_status_.end())
        return;

    // always set up, avoid some bugs
    file->second = static_cast<PathStatus>(status);
    setBadgeIdentifierFor(path, static_cast<PathStatus>(status));
}

- (IBAction)showLockedByAction:(id)sender {
    NSArray *items =
        [[FIFinderSyncController defaultController] selectedItemURLs];
    if (![items count])
        return;
    NSURL *item = items.firstObject;

    // do it in another thread
    std::string path =
        item.path.precomposedStringWithCanonicalMapping.UTF8String;
    dispatch_async(self.client_command_queue_, ^{
      client_->doSendCommandWithPath(FinderSyncClient::DoShowFileLockedBy,
                                     path.c_str());
    });
}

@end
