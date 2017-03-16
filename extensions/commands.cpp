#include "ext-common.h"

#include "log.h"
#include "applet-connection.h"
#include "ext-utils.h"

#include "commands.h"

namespace seafile {

uint64_t reposInfoTimestamp = 0;

std::string toString(Status st) {
    switch (st) {
    case InvalidStatus:
        return "invalid status";
    case NoStatus:
        return "nostatus";
    case Paused:
        return "paused";
    case Normal:
        return "synced";
    case Syncing:
        return "syncing";
    case Error:
        return "error";
    case ReadOnly:
        return "readonly";
    case LockedByMe:
        return "locked by me";
    case LockedByOthers:
        return "locked by someone else";
    case N_Status:
        return "";
    }
    return "";
}


GetShareLinkCommand::GetShareLinkCommand(const std::string path)
    : AppletCommand<void>("get-share-link"),
      path_(path)
{
}

std::string GetShareLinkCommand::serialize()
{
    return path_;
}

GetAdvancedShareLinkCommand::GetAdvancedShareLinkCommand(const std::string path)
    : AppletCommand<void>("get-advanced-share-link"),
      path_(path)
{
}

std::string GetAdvancedShareLinkCommand::serialize()
{
    return path_;
}

GetInternalLinkCommand::GetInternalLinkCommand(const std::string path)
    : AppletCommand<void>("get-internal-link"),
      path_(path)
{
}

std::string GetInternalLinkCommand::serialize()
{
    return path_;
}

ListReposCommand::ListReposCommand()
    : AppletCommand<RepoInfoList>("list-repos")
{
}

std::string ListReposCommand::serialize()
{
    return "";
}

bool ListReposCommand::parseResponse(const std::string& raw_resp,
                                     RepoInfoList* infos)
{
    // seaf_ext_log ("ListReposCommand: raw_resp is %s\n", raw_resp.c_str());

    std::vector<std::string> lines = utils::split(raw_resp, '\n');
    if (lines.empty()) {
        return true;
    }
    for (size_t i = 0; i < lines.size(); i++) {
        std::string line = lines[i];
        std::string repo_dir = utils::normalizedPath(line);
        seaf_ext_log ("repo dir: %s\n", repo_dir.c_str());
        infos->push_back(RepoInfo(repo_dir));
    }

    reposInfoTimestamp = utils::currentMSecsSinceEpoch();
    return true;
}

GetStatusCommand::GetStatusCommand(const std::string& path)
    : AppletCommand<Status>("get-file-status"),
    path_(path)
{
}

std::string GetStatusCommand::serialize()
{
    return path_;
}

bool GetStatusCommand::parseResponse(const std::string& raw_resp,
                                     Status *status)
{
    // seaf_ext_log ("raw_resp is %s\n", raw_resp.c_str());

    if (raw_resp == "syncing") {
        *status = Syncing;
    } else if (raw_resp == "synced") {
        *status = Normal;
    } else if (raw_resp == "error") {
        *status = Error;
    } else if (raw_resp == "paused") {
        *status = Paused;
    } else if (raw_resp == "readonly") {
        *status = ReadOnly;
    } else if (raw_resp == "locked") {
        *status = LockedByOthers;
    } else if (raw_resp == "locked_by_me") {
        *status = LockedByMe;
    } else if (raw_resp == "ignored") {
        *status = NoStatus;
    } else {
        *status = NoStatus;

        seaf_ext_log ("[GetStatusCommand] status for %s is %s, raw_resp is %s\n",
                      path_.c_str(),
                      seafile::toString(*status).c_str(), raw_resp.c_str());
    }

    return true;
}

LockFileCommand::LockFileCommand(const std::string& path)
    : AppletCommand<void>("lock-file"),
    path_(path)
{
}

std::string LockFileCommand::serialize()
{
    return path_;
}

UnlockFileCommand::UnlockFileCommand(const std::string& path)
    : AppletCommand<void>("unlock-file"),
    path_(path)
{
}

std::string UnlockFileCommand::serialize()
{
    return path_;
}

PrivateShareCommand::PrivateShareCommand(const std::string& path, bool to_group)
    : AppletCommand<void>(to_group ? "private-share-to-group"
                                   : "private-share-to-user"),
      path_(path)
{
}

std::string PrivateShareCommand::serialize()
{
    return path_;
}

ShowHistoryCommand::ShowHistoryCommand(const std::string& path)
    : AppletCommand<void>("show-history"),
      path_(path)
{
}

std::string ShowHistoryCommand::serialize()
{
    return path_;
}

} // namespace seafile
