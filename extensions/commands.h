#ifndef SEAFILE_EXTENSION_APPLET_COMMANDS_H
#define SEAFILE_EXTENSION_APPLET_COMMANDS_H

#include <string>
#include <vector>

#include "applet-connection.h"

namespace seafile {

static std::string mount_point; // The mount_point of seadrive disk, deafult to "S:".

enum Status {
    InvalidStatus = 0,
    NoStatus,
    Normal,
    Syncing,
    Error,
    LockedByMe,
    LockedByOthers,
    ReadOnly,
    PartialSynced,
    N_Status,
};

class RepoInfo
{
public:
    std::string topdir;

    bool support_file_lock;
    bool support_private_share;

    RepoInfo() {}

    RepoInfo(const std::string topdir)
        : topdir(topdir)
    {
    }

};

std::string toString(Status st);

typedef std::vector<RepoInfo> RepoInfoList;
typedef std::vector<std::string> RepoDirs;

/**
 * Abstract base class for all commands sent to seafile applet.
 */
template<class T>
class AppletCommand {
public:
    AppletCommand(std::string name) : name_(name) {}

    /**
     * send the command to seafile client, don't need the response
     */
    void send()
    {
        AppletConnection::instance()->sendCommand(formatRequest());
    }

    std::string formatRequest()
    {
        std::string body = serialize();
        if (body.empty()) {
            return name_;
        } else {
            return name_ + "\t" + body;
        }
    }

    /**
     * send the command to seafile client, and wait for the response
     */
    bool sendAndWait(T *resp)
    {
        std::string raw_resp;
        if (!AppletConnection::instance()->sendCommandAndWait(formatRequest(), &raw_resp)) {
            return false;
        }

        return parseResponse(raw_resp, resp);
    }

protected:
    /**
     * Prepare this command for sending through the pipe
     */
    virtual std::string serialize() = 0;

    /**
     * Parse response from seafile applet. Commands that don't need the
     * respnse can inherit the implementation of the base class, which does
     * nothing.
     */
    virtual bool parseResponse(const std::string& raw_resp, T *resp)
    {
        return true;
    }

private:
    std::string name_;
};


class GetShareLinkCommand : public AppletCommand<void> {
public:
    GetShareLinkCommand(const std::string path);

protected:
    std::string serialize();

private:
    std::string path_;
};

class GetInternalLinkCommand : public AppletCommand<void> {
public:
    GetInternalLinkCommand(const std::string path);

protected:
    std::string serialize();

private:
    std::string path_;
};


class ListReposCommand : public AppletCommand<RepoInfoList> {
public:
    ListReposCommand();

protected:
    std::string serialize();

    bool parseResponse(const std::string& raw_resp, RepoInfoList *infos);
};

class GetStatusCommand : public AppletCommand<Status> {
public:
    GetStatusCommand(const std::string& path);

protected:
    std::string serialize();

    bool parseResponse(const std::string& raw_resp, Status *status);

private:
    std::string path_;
};

class LockFileCommand : public AppletCommand<void> {
public:
    LockFileCommand(const std::string& path);

protected:
    std::string serialize();

private:
    std::string path_;
};

class UnlockFileCommand : public AppletCommand<void> {
public:
    UnlockFileCommand(const std::string& path);

protected:
    std::string serialize();

private:
    std::string path_;
};

class PrivateShareCommand : public AppletCommand<void> {
public:
    PrivateShareCommand(const std::string& path, bool to_group);

protected:
    std::string serialize();

private:
    std::string path_;
    bool to_group;
};

class ShowHistoryCommand : public AppletCommand<void> {
public:
    ShowHistoryCommand(const std::string& path);

protected:
    std::string serialize();

private:
    std::string path_;
};

}

#endif // SEAFILE_EXTENSION_APPLET_COMMANDS_H
