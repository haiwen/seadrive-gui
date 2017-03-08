#include <QObject>
#include <QStringList>

#include "utils/utils.h"
#include "utils/json-utils.h"

#include "sync-error.h"

SyncError SyncError::fromJSON(const json_t *root)
{
    SyncError error;
    Json json(root);

    error.repo_id = json.getString("repo_id");
    error.repo_name = json.getString("repo_name");
    error.path = json.getString("path");
    error.error_id = json.getLong("error_id");
    error.timestamp = json.getLong("timestamp");

    error.translateErrorStr();

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
/* #define SYNC_ERROR_ID_FILE_LOCKED 2 */
/* #define SYNC_ERROR_ID_INVALID_PATH 3 */
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

#define SYNC_ERROR_ID_GENERAL_ERROR             100


void SyncError::translateErrorStr()
{
    readable_time_stamp = translateCommitTime(timestamp);

    switch (error_id) {
    case SYNC_ERROR_ID_FILE_LOCKED_BY_APP:
        error_str = QObject::tr("File is locked by another application");
        break;
    case SYNC_ERROR_ID_FOLDER_LOCKED_BY_APP:
        error_str = QObject::tr("Folder is locked by another application");
        break;
    case SYNC_ERROR_ID_INDEX_ERROR:
        error_str = QObject::tr("Error when indexing");
        break;
    default:
        // unreachable
        qWarning("unknown sync error id %d", error_id);
        error_str = "";
    }

    printf ("sync error: %s\n", error_str.toUtf8().data());
}
