#ifndef SEADRIVE_GUI_RPC_TRANSFER_PROGRESS_H
#define SEADRIVE_GUI_RPC_TRANSFER_PROGRESS_H

#include <QString>
#include <QList>
#include <jansson.h>

enum TransferType {
    UPLOAD = 0,
    DOWNLOAD,
};

struct TransferringInfo {
    QString file_path;
    QString server;
    QString username;
    quint64 transferred_bytes;
    quint64 total_bytes;
};

struct TransferredInfo {
    QString file_path;
    QString server;
    QString username;
    // quint64 total_bytes = 0;
};

class TransferProgress {
public:
    QList<TransferringInfo> uploading_files, downloading_files;
    QList<TransferredInfo> uploaded_files, downloaded_files;
    int n_pending_files;

    static void fromJSON(const json_t *upload,
                         const json_t *download,
                         TransferProgress& transfer_progress);
};

#endif // SEADRIVE_GUI_RPC_TRANSFER_PROGRESS_H
