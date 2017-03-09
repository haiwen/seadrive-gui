#ifndef SEADRIVE_GUI_RPC_SYNC_ERROR_H
#define SEADRIVE_GUI_RPC_SYNC_ERROR_H

#include <QString>
#include <QList>
#include <QMetaType>

#include <jansson.h>

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

    static QList<SyncError> listFromJSON(const json_t *objlist);
    static SyncError fromJSON(const json_t *objlist);

    static QString syncErrorIdToErrorStr(int errr_id, const QString& path);
    void translateErrorStr();

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
