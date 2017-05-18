#ifndef SEADRIVE_GUI_RPC_TRANSFER_PROGRESS_H
#define SEADRIVE_GUI_RPC_TRANSFER_PROGRESS_H

#include <QString>
#include <QList>
#include <jansson.h>

enum TransferType {
    UPLOAD,
    DOWNLOAD,
};

struct TransferringInfo {
    QString file_path;
    quint64 transferred_bytes;
    quint64 total_bytes;
};

struct TransferredInfo {
    QString file_path;
    // quint64 total_bytes = 0;
};

class TransferProgress {
public:
    QList<TransferringInfo> uploading_files_, downloading_files_;
    QList<TransferredInfo> uploaded_files_, downloaded_files_;

    static TransferProgress fromJSON(const json_t *upload,
                                     const json_t *download);
};

#endif // SEADRIVE_GUI_RPC_TRANSFER_PROGRESS_H
