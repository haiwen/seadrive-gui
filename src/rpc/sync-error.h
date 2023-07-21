#ifndef SEADRIVE_GUI_RPC_SYNC_ERROR_H
#define SEADRIVE_GUI_RPC_SYNC_ERROR_H

#include <QString>
#include <QList>
#include <QMetaType>

#include <jansson.h>

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
#define SYNC_ERROR_ID_INVALID_PATH_ON_WINDOWS   29
#define SYNC_ERROR_ID_TOO_MANY_FILES            30
#define SYNC_ERROR_ID_BLOCK_MISSING             31
#define SYNC_ERROR_ID_GENERAL_ERROR             32


class SyncError {
public:
    QString repo_id;
    QString repo_name;
    QString path;
    qint64 timestamp;
    int error_id;

    // Generated fields.
    QString readable_time_stamp;
    QString error_str;

    SyncError() {
        timestamp = -1;
        error_id = -1;
    }

    static QList<SyncError> listFromJSON(const json_t *objlist);
    static SyncError fromJSON(const json_t *objlist);

    static QString syncErrorIdToErrorStr(int errr_id, const QString& path);
    void translateErrorStr();

    bool isGlobalError() const;

    bool isValid() const { return error_id >= 0; }

    bool operator==(const SyncError& rhs) const {
        return repo_id == rhs.repo_id
            && repo_name == rhs.repo_name
            && path == rhs.path
            && error_id == rhs.error_id
            && timestamp == rhs.timestamp;
    }

    bool operator!=(const SyncError& rhs) const {
        return !(*this == rhs);
    }
};

#endif // SEADRIVE_GUI_RPC_SYNC_ERROR_H
