#include <QObject>
#include <QStringList>

#include "utils/utils.h"
#include "utils/file-utils.h"
#include "utils/json-utils.h"

#include "sync-error.h"

SyncError SyncError::fromJSON(const json_t *root)
{
    SyncError error;
    Json json(root);

    error.repo_id = json.getString("repo_id");
    error.repo_name = json.getString("repo_name");
    error.path = json.getString("path");
    error.error_id = json.getLong("err_id");
    error.timestamp = json.getLong("timestamp");

    error.translateErrorStr();

    // printf ("sync error: %s\n", error.error_str.toUtf8().data());

    return error;
}

QList<SyncError> SyncError::listFromJSON(const json_t *json)
{
    QList<SyncError> errors;
    for (size_t i = 0; i < json_array_size(json); i++) {
        SyncError error = fromJSON(json_array_get(json, i));
        errors.push_back(error);
    }

    return errors;
}

#define SYNC_ERROR_ID_FILE_LOCKED_BY_APP        0
#define SYNC_ERROR_ID_FOLDER_LOCKED_BY_APP      1
#define SYNC_ERROR_ID_FILE_LOCKED 2
#define SYNC_ERROR_ID_INVALID_PATH 3
#define SYNC_ERROR_ID_INDEX_ERROR               4
#define SYNC_ERROR_ID_ACCESS_DENIED             5
#define SYNC_ERROR_ID_QUOTA_FULL                6
#define SYNC_ERROR_ID_NETWORK                   7
#define SYNC_ERROR_ID_RESOLVE_PROXY             8
#define SYNC_ERROR_ID_RESOLVE_HOST              9
#define SYNC_ERROR_ID_CONNECT                   10
#define SYNC_ERROR_ID_SSL                       11
#define SYNC_ERROR_ID_TX                        12
#define SYNC_ERROR_ID_TX_TIMEOUT                13
#define SYNC_ERROR_ID_UNHANDLED_REDIRECT        14
#define SYNC_ERROR_ID_SERVER                    15
#define SYNC_ERROR_ID_LOCAL_DATA_CORRUPT        16
#define SYNC_ERROR_ID_WRITE_LOCAL_DATA          17
#define SYNC_ERROR_ID_PERM_NOT_SYNCABLE         18
#define SYNC_ERROR_ID_NO_WRITE_PERMISSION       19
#define SYNC_ERROR_ID_FOLDER_PERM_DENIED        20
#define SYNC_ERROR_ID_PATH_END_SPACE_PERIOD     21
#define SYNC_ERROR_ID_PATH_INVALID_CHARACTER    22
#define SYNC_ERROR_ID_UPDATE_TO_READ_ONLY_REPO  23
#define SYNC_ERROR_ID_CONFLICT                  24
#define SYNC_ERROR_ID_UPDATE_NOT_IN_REPO        25
#define SYNC_ERROR_ID_LIBRARY_TOO_LARGE         26
#define SYNC_ERROR_ID_MOVE_NOT_IN_REPO          27
#define SYNC_ERROR_ID_DEL_CONFIRMATION_PENDING  28

#define SYNC_ERROR_ID_GENERAL_ERROR             100

QString SyncError::syncErrorIdToErrorStr(int error_id, const QString& path)
{
    bool has_path = !path.isEmpty();
    QString file;
    if (has_path) {
        file = ::getBaseName(path);
    }
    switch (error_id) {
    case SYNC_ERROR_ID_FILE_LOCKED_BY_APP: {
        if (has_path) {
            return QObject::tr("File %1 is locked by other programs").arg(file);
        } else {
            return QObject::tr("Some file is locked by other programs");
        }
    }
    case SYNC_ERROR_ID_FOLDER_LOCKED_BY_APP: {
        if (has_path) {
            return QObject::tr("Folder %1 is locked by other programs").arg(file);
        } else {
            return QObject::tr("Some folder is locked by other programs");
        }
    }
    case SYNC_ERROR_ID_FILE_LOCKED: {
        if (has_path) {
            return QObject::tr("File %1 is locked by another user").arg(file);
        } else {
            return QObject::tr("Some file is locked by another user");
        }
    }
    case SYNC_ERROR_ID_INVALID_PATH: {
        if (has_path) {
            return QObject::tr("Invalid path %1").arg(file);
        } else {
            return QObject::tr("Trying to access an invalid path");
        }
    }
    case SYNC_ERROR_ID_INDEX_ERROR: {
        if (has_path) {
            return QObject::tr("Error when indexing file %1").arg(file);
        } else {
            return QObject::tr("Error when indexing files");
        }
    }
    case SYNC_ERROR_ID_ACCESS_DENIED: {
        return QObject::tr("You don't have enough permission for this library");
    }
    case SYNC_ERROR_ID_QUOTA_FULL:
        return QObject::tr("The storage quota has been used up");
    case SYNC_ERROR_ID_NETWORK:
        return QObject::tr("Network error");
    case SYNC_ERROR_ID_RESOLVE_PROXY:
        return QObject::tr("Failed to resolve network proxy");
    case SYNC_ERROR_ID_RESOLVE_HOST:
        return QObject::tr("Failed to resolve remote server");
    case SYNC_ERROR_ID_CONNECT:
        return QObject::tr("Failed to connect to server");
    case SYNC_ERROR_ID_SSL:
        return QObject::tr("SSL error");
    case SYNC_ERROR_ID_TX:
        return QObject::tr("Error in network transmission");
    case SYNC_ERROR_ID_TX_TIMEOUT:
        return QObject::tr("Timeout in network transmission");
    case SYNC_ERROR_ID_UNHANDLED_REDIRECT:
        return QObject::tr("Failed to handle http redirection");
    case SYNC_ERROR_ID_SERVER:
        return QObject::tr("Server internal error");
    case SYNC_ERROR_ID_LOCAL_DATA_CORRUPT:
        return QObject::tr("Local data is corrupt");
    case SYNC_ERROR_ID_WRITE_LOCAL_DATA:
        return QObject::tr("Failed to write local data");
    case SYNC_ERROR_ID_PERM_NOT_SYNCABLE:
        return QObject::tr("Syncing is denied by cloud-only permission settings");
    case SYNC_ERROR_ID_NO_WRITE_PERMISSION:
        return QObject::tr("Do not have write permission to the library");
    case SYNC_ERROR_ID_FOLDER_PERM_DENIED:
        return QObject::tr("Update to file %1 denied by folder permission setting").arg(file);
    case SYNC_ERROR_ID_PATH_END_SPACE_PERIOD:
        return QObject::tr("Path %1 ends with space or period character").arg(file);
    case SYNC_ERROR_ID_PATH_INVALID_CHARACTER:
        return QObject::tr("Path %1 contains invalid characters like '|' or ':'").arg(file);
    case SYNC_ERROR_ID_UPDATE_TO_READ_ONLY_REPO:
        return QObject::tr("Created or updated a file %1 in a non-writable library or folder").arg(file);
    case SYNC_ERROR_ID_CONFLICT:
        return QObject::tr("Concurrent updates to file %1. File is saved as conflict file").arg(file);
    case SYNC_ERROR_ID_UPDATE_NOT_IN_REPO:
        return QObject::tr("%1 is not added to a library and will not be uploaded").arg(file);
    case SYNC_ERROR_ID_LIBRARY_TOO_LARGE:
        return QObject::tr("Library is too large to sync.");
#if defined(Q_OS_WIN32)
    case SYNC_ERROR_ID_MOVE_NOT_IN_REPO:
        return QObject::tr("You can only move files/folders within a library or from one library to another library");
#endif
    case SYNC_ERROR_ID_DEL_CONFIRMATION_PENDING:
        return QObject::tr("Waiting for confirmation to delete files");
    case SYNC_ERROR_ID_GENERAL_ERROR:
    default:
        return QObject::tr("Unknown error");
    }
}

void SyncError::translateErrorStr()
{
    readable_time_stamp = translateCommitTime(timestamp);
    error_str = syncErrorIdToErrorStr(error_id, path);
}

bool SyncError::isGlobalError() const
{
    return repo_id.isEmpty();
}
