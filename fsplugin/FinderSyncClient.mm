//
//  FinderSyncClient.m
//  seadrive-fsplugin
//
//  Created by Chilledheart on 1/10/15.
//  Copyright (c) 2015 Haiwen. All rights reserved.
//

#import "FinderSyncClient.h"
#include <cstdint>
#include <sstream>
#include <servers/bootstrap.h>

#include "../src/utils/stl.h"


#if !__has_feature(objc_arc)
#error this file must be built with ARC support
#endif

static NSString *const kFinderSyncMachPort =
    @"com.seafile.seadrive.findersync.machport";

static constexpr int kWatchDirMax = 100;
static constexpr int kPathMaxSize = 1024;
static constexpr uint32_t kFinderSyncProtocolVersion = 0x00000004;
static volatile int32_t message_id_ =
    100; // we start from 100, the number below than 100 is reserved
bool internal_link_supported = false;

//
// MachPort Message
// - mach_msg_header_t command
// - uint32_t version
// - uint32_t command
// - body
// - mach_msg_trailer_t trailer (for rcv only)
//
//

static std::vector<std::string> split(const std::string &s, char delim)
{
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, delim) && !item.empty()) {
        elems.push_back(item);
    }
    return elems;
}


// buffer
// <-char[36]-><----- char? ------------>1   1      1
// <-repo_id--><---- [worktree name] --->0<[status]>0
static std::vector<std::string>
*deserializeWatchSet(const std::string& raw_resp)
{
    std::vector<std::string> *repos = new std::vector<std::string>();
    // const char *const end = buffer + size - 1;
    // const char *pos;
    // while (buffer != end) {
    //     unsigned worktree_size;
    //     uint8_t status;
    //     const char *repo_id = buffer;
    //     buffer += 36;
    //     pos = buffer;

    //     while (*pos != '\0' && pos != end)
    //         ++pos;
    //     worktree_size = pos - buffer;
    //     pos += 2;
    //     if (pos > end || *pos != '\0')
    //         break;

    //     status = *(pos - 1);
    //     if (status >= LocalRepo::MAX_SYNC_STATE) {
    //         status = LocalRepo::SYNC_STATE_UNSET;
    //     }

    //     repos->emplace_back(std::string(repo_id, 36),
    //                         std::string(buffer, worktree_size),
    //                         static_cast<LocalRepo::SyncState>(status));
    //     buffer = ++pos;
    // }
    std::vector<std::string> parts = split(raw_resp, '\t');
    auto repos_resp = parts[0];
    if (parts.size() > 1) {
        internal_link_supported = parts[1] == "internal-link-supported";
    }

    std::vector<std::string> lines = split(repos_resp, '\n');
    for (size_t i = 0; i < lines.size(); i++) {
        std::string line = lines[i];
        repos->emplace_back(line);
    }

    return repos;
}

struct mach_msg_command_send_t {
    mach_msg_header_t header;
    uint32_t version;
    uint32_t command;
    // used only in DoShareLink
    char repo[36];
    char body[kPathMaxSize];
};

// This is useless. The extension runs in a sandbox environment, and has no
// permission to access the file system.
#if 0
static void listSubDirectories(const std::string& parentDir, std::vector<std::string> *subdirs)
{
    NSString *sourcePath = [NSString stringWithUTF8String:parentDir.c_str()];
    NSArray* dirs = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:sourcePath
                                                                        error:NULL];
    if (!dirs) {
        return;
    }
    [dirs enumerateObjectsUsingBlock:^(id obj, NSUInteger idx, BOOL *stop) {
            NSString *filename = (NSString *)obj;
            NSArray *parts = [NSArray arrayWithObjects: sourcePath, filename, nil];
            NSString *fullpath = [NSString pathWithComponents:parts];

            NSDictionary *attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:fullpath
                                                                                   error:NULL];
            if (attrs && [attrs objectForKey:NSFileType] == NSFileTypeDirectory) {
                subdirs->push_back(fullpath.UTF8String);
            }
        }];
}
#endif

struct mach_msg_file_status_rcv_t {
    mach_msg_header_t header;
    uint32_t version;
    uint32_t command;
    uint32_t status;
    mach_msg_trailer_t trailer;
};

FinderSyncClient::FinderSyncClient(FinderSync *parent)
    : parent_(parent), local_port_(MACH_PORT_NULL),
      remote_port_(MACH_PORT_NULL) {}

FinderSyncClient::~FinderSyncClient() {
    if (local_port_) {
        mach_port_mod_refs(mach_task_self(), local_port_,
                           MACH_PORT_RIGHT_RECEIVE, -1);
    }
    if (remote_port_) {
        NSLog(@"disconnecting from SeaDrive Client");
        mach_port_deallocate(mach_task_self(), remote_port_);
    }
}

void FinderSyncClient::connectionBecomeInvalid() {
    /* clean up old connection stage! */
    if (local_port_) {
        mach_port_mod_refs(mach_task_self(), local_port_,
                           MACH_PORT_RIGHT_RECEIVE, -1);
        local_port_ = MACH_PORT_NULL;
    }
    if (remote_port_) {
        NSLog(@"lost connection with SeaDrive Client");
        mach_port_deallocate(mach_task_self(), remote_port_);
        dispatch_async(dispatch_get_main_queue(), ^{
          [parent_ updateWatchSet:nil];
        });
        remote_port_ = MACH_PORT_NULL;
    }
}

bool FinderSyncClient::connect() {
    if (!local_port_) {
        // Create a local port.
        kern_return_t kr = mach_port_allocate(
            mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &local_port_);
        if (kr != KERN_SUCCESS) {
            NSLog(@"unable to create connection");
            return false;
        }
    }

    if (!remote_port_) {
        // connect to the mach_port
        kern_return_t kr = bootstrap_look_up(
            bootstrap_port,
            [kFinderSyncMachPort cStringUsingEncoding:NSASCIIStringEncoding],
            &remote_port_);

        if (kr != KERN_SUCCESS) {
            return false;
        }
        NSLog(@"connected to SeaDrive Client");
    }

    return true;
}

void FinderSyncClient::getWatchSet() {
    if ([NSThread isMainThread]) {
        NSLog(@"%s isn't supported to be called from main thread",
              __PRETTY_FUNCTION__);
        return;
    }
    if (!connect())
        return;

    // if (true) {
    //     std::vector<std::string> *repos = new std::vector<std::string>();
    //     listSubDirectories("/Users/lin/SeaDrive", repos);
    //     dispatch_async(dispatch_get_main_queue(), ^{
    //             [parent_ updateWatchSet:repos];
    //             delete repos;
    //         });
    //     return;
    // }

    mach_msg_command_send_t msg;
    const int32_t recv_msgh_id = OSAtomicAdd32(2, &message_id_) - 1;
    bzero(&msg, sizeof(mach_msg_header_t));
    msg.header.msgh_id = recv_msgh_id - 1;
    msg.header.msgh_local_port = local_port_;
    msg.header.msgh_remote_port = remote_port_;
    msg.header.msgh_size = sizeof(msg);
    msg.header.msgh_bits =
        MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
    msg.version = kFinderSyncProtocolVersion;
    msg.command = GetWatchSet;
    // send a message and wait for the reply
    kern_return_t kr = mach_msg(&msg.header,                       /* header*/
                                MACH_SEND_MSG | MACH_SEND_TIMEOUT, /*option*/
                                sizeof(msg),                       /*send size*/
                                0,               /*receive size*/
                                local_port_,     /*receive port*/
                                100,             /*timeout, in milliseconds*/
                                MACH_PORT_NULL); /*no notification*/
    if (kr != MACH_MSG_SUCCESS) {
        if (kr == MACH_SEND_INVALID_DEST) {
            connectionBecomeInvalid();
            return;
        }
        NSLog(@"failed to send request to SeaDrive Client");
        NSLog(@"mach error %s", mach_error_string(kr));
        connectionBecomeInvalid();
        return;
    }
    mach_msg_destroy(&msg.header);

    utils::BufferArray recv_msg;
    recv_msg.resize(4096);
    mach_msg_header_t *recv_msg_header =
        reinterpret_cast<mach_msg_header_t *>(recv_msg.data());
    bzero(recv_msg.data(), sizeof(mach_msg_header_t));
    recv_msg_header->msgh_local_port = local_port_;
    recv_msg_header->msgh_remote_port = remote_port_;
    // recv_msg.header.msgh_size = sizeof(recv_msg);
    // receive the reply
    kr = mach_msg(recv_msg_header,                                  /* header*/
                  MACH_RCV_MSG | MACH_RCV_TIMEOUT | MACH_RCV_LARGE, /*option*/
                  0,               /*send size*/
                  recv_msg.size(), /*receive size*/
                  local_port_,     /*receive port*/
                  1000,             /*timeout, in milliseconds*/
                  MACH_PORT_NULL); /*no notification*/
    // retry
    if (kr == MACH_RCV_TOO_LARGE) {
        recv_msg.resize(recv_msg_header->msgh_size +
                        sizeof(mach_msg_trailer_t));
        recv_msg_header =
            reinterpret_cast<mach_msg_header_t *>(recv_msg.data());

        kr = mach_msg(recv_msg_header,                 /* header*/
                      MACH_RCV_MSG | MACH_RCV_TIMEOUT, /*option*/
                      0,                               /*send size*/
                      recv_msg.size(),                 /*receive size*/
                      local_port_,                     /*receive port*/
                      100,             /*timeout, in milliseconds*/
                      MACH_PORT_NULL); /*no notification*/
    }
    if (kr != MACH_MSG_SUCCESS) {
        NSLog(@"failed to receive SeaDrive Client's reply");
        NSLog(@"mach error %s", mach_error_string(kr));
        connectionBecomeInvalid();
        return;
    }

    if (recv_msg_header->msgh_id != recv_msgh_id) {
        NSLog(@"mach error unmatched message id %d, expected %d",
              recv_msg_header->msgh_id, recv_msgh_id);
        connectionBecomeInvalid();
        return;
    }
    const char *body = recv_msg.data() + sizeof(mach_msg_header_t);
    uint32_t body_size = (recv_msg_header->msgh_size - sizeof(mach_msg_header_t));

    std::unique_ptr<char[]> buf(new char[body_size+1]);
    memcpy(buf.get(), body, body_size);
    buf.get()[body_size] = 0;

    std::vector<std::string> *repos = deserializeWatchSet(buf.get());
    dispatch_async(dispatch_get_main_queue(), ^{
      [parent_ updateWatchSet:repos];
      delete repos;
    });
    mach_msg_destroy(recv_msg_header);
}

void FinderSyncClient::doSendCommandWithPath(CommandType command,
                                             const char *fileName) {
    if ([NSThread isMainThread]) {
        NSLog(@"%s isn't supported to be called from main thread",
              __PRETTY_FUNCTION__);
        return;
    }
    if (!connect())
        return;
    mach_msg_command_send_t msg;
    bzero(&msg, sizeof(msg));
    msg.header.msgh_id = OSAtomicIncrement32(&message_id_) - 1;
    msg.header.msgh_local_port = MACH_PORT_NULL;
    msg.header.msgh_remote_port = remote_port_;
    msg.header.msgh_size = sizeof(msg);
    msg.header.msgh_bits = MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_COPY_SEND);
    strncpy(msg.body, fileName, kPathMaxSize);
    msg.version = kFinderSyncProtocolVersion;
    msg.command = command;
    // send a message only
    kern_return_t kr = mach_msg_send(&msg.header);
    if (kr != MACH_MSG_SUCCESS) {
        if (kr == MACH_SEND_INVALID_DEST) {
            connectionBecomeInvalid();
            return;
        }
        NSLog(@"failed to send sharing link request for %s", fileName);
        NSLog(@"mach error %s from msg id %d", mach_error_string(kr),
              msg.header.msgh_id);
        connectionBecomeInvalid();
        return;
    }
    mach_msg_destroy(&msg.header);
}

void FinderSyncClient::doGetFileStatus(const char* path) {
    if ([NSThread isMainThread]) {
        NSLog(@"%s isn't supported to be called from main thread",
              __PRETTY_FUNCTION__);
        return;
    }
    if (!connect())
        return;
    mach_msg_command_send_t msg;
    const int32_t recv_msgh_id = OSAtomicAdd32(2, &message_id_) - 1;
    bzero(&msg, sizeof(mach_msg_header_t));
    msg.header.msgh_id = recv_msgh_id - 1;
    msg.header.msgh_local_port = local_port_;
    msg.header.msgh_remote_port = remote_port_;
    msg.header.msgh_size = sizeof(msg);
    msg.header.msgh_bits =
        MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, MACH_MSG_TYPE_MAKE_SEND_ONCE);
    strncpy(msg.body, path, kPathMaxSize);
    msg.version = kFinderSyncProtocolVersion;
    msg.command = DoGetFileStatus;
    // send a message and wait for the reply
    kern_return_t kr = mach_msg(&msg.header,                       /* header*/
                                MACH_SEND_MSG | MACH_SEND_TIMEOUT, /*option*/
                                sizeof(msg),                       /*send size*/
                                0,               /*receive size*/
                                local_port_,     /*receive port*/
                                100,             /*timeout, in milliseconds*/
                                MACH_PORT_NULL); /*no notification*/
    if (kr != MACH_MSG_SUCCESS) {
        if (kr == MACH_SEND_INVALID_DEST) {
            connectionBecomeInvalid();
            return;
        }
        NSLog(@"failed to send request to SeaDrive Client");
        NSLog(@"mach error %s", mach_error_string(kr));
        connectionBecomeInvalid();
        return;
    }
    mach_msg_destroy(&msg.header);

    mach_msg_file_status_rcv_t recv_msg;
    mach_msg_header_t *recv_msg_header =
        reinterpret_cast<mach_msg_header_t *>(&recv_msg);
    bzero(&recv_msg, sizeof(mach_msg_header_t));
    recv_msg_header->msgh_local_port = local_port_;
    recv_msg_header->msgh_remote_port = remote_port_;
    // recv_msg.header.msgh_size = sizeof(recv_msg);
    // receive the reply
    kr = mach_msg(recv_msg_header,                    /* header*/
                  MACH_RCV_MSG | MACH_RCV_TIMEOUT,    /*option*/
                  0,                                  /*send size*/
                  sizeof(mach_msg_file_status_rcv_t), /*receive size*/
                  local_port_,                        /*receive port*/
                  100,             /*timeout, in milliseconds*/
                  MACH_PORT_NULL); /*no notification*/
    if (kr != MACH_MSG_SUCCESS) {
        NSLog(@"failed to receive SeaDrive Client's reply");
        NSLog(@"mach error %s", mach_error_string(kr));
        connectionBecomeInvalid();
        return;
    }
    if (recv_msg_header->msgh_id != recv_msgh_id) {
        NSLog(@"mach error unmatched message id %d, expected %d",
              recv_msg_header->msgh_id, recv_msgh_id);
        connectionBecomeInvalid();
        return;
    }
    uint32_t status = recv_msg.status;
    if (status >= PathStatus::MAX_SYNC_STATUS)
        status = PathStatus::SYNC_STATUS_NONE;

    // Copy the path to a std::string so it can would captured in the block.
    //
    // If we use the (const char*) `path` in the block, the content of `path` may
    // become invalid when the block is executed in the main thread.
    std::string path_in_block = path;
    dispatch_async(dispatch_get_main_queue(), ^{
      [parent_ updateFileStatus:path_in_block.c_str()
                         status:status];
    });
    mach_msg_destroy(recv_msg_header);
}
