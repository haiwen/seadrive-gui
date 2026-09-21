#include <QMap>
#include <QVariant>

#include "utils/json-utils.h"
#include "utils/utils.h"

#include "transfer-progress.h"

namespace {

void getTransferringListFromJSON(
    const json_t *json, TransferType type,
    QList<TransferringInfo> *list)
{
    QString json_object_name;
    QString transferred_name;
    QString total_bytes_name;

    if (type == UPLOAD) {
        json_object_name = "uploading_files";
        transferred_name = "uploaded";
        total_bytes_name = "total_upload";
    } else {
        json_object_name = "downloading_files";
        transferred_name = "downloaded";
        total_bytes_name = "total_download";
    }

    json_t* transferring_array = json_object_get(
        json, json_object_name.toUtf8().data());

    json_t* transferring_object;
    json_error_t error;
    size_t index;
    json_array_foreach(transferring_array, index, transferring_object) {
        QMap<QString, QVariant> dict =
            mapFromJSON(transferring_object, &error);
        TransferringInfo transferring_info;
        transferring_info.file_path =
            dict.value("file_path").toString();
        transferring_info.server =
            dict.value("server").toString();
        transferring_info.username =
            dict.value("username").toString();
        transferring_info.transferred_bytes =
            dict.value(transferred_name.toUtf8().data()).toULongLong();
        transferring_info.total_bytes =
            dict.value(total_bytes_name.toUtf8().data()).toULongLong();
        list->push_back(transferring_info);
    }
}

void getPendingFilesFromJSON(
    const json_t *json,
    int *total)
{
    QString json_object_name = "pending_files";

    json_t* n_pending= json_object_get(
        json, json_object_name.toUtf8().data());
    if (n_pending)
        *total = json_integer_value (n_pending);
}

void getTransferredListFromJSON(
    const json_t *json, TransferType type,
    QList<TransferredInfo> *list)
{
    QString json_object_name;

    if (type == UPLOAD) {
        json_object_name = "uploaded_files";
    } else {
        json_object_name = "downloaded_files";
    }

    json_t* transferred_array = json_object_get(
        json, json_object_name.toUtf8().data());

    json_t* transferred_object;
    json_error_t error;
    size_t index;
    json_array_foreach(transferred_array, index, transferred_object) {
        QMap<QString, QVariant> dict =
            mapFromJSON(transferred_object, &error);
        TransferredInfo transferred_info;
        transferred_info.file_path =
            dict.value("file_path").toString();
        transferred_info.server =
            dict.value("server").toString();
        transferred_info.username =
            dict.value("username").toString();
        list->push_back(transferred_info);
    }
}

} // namespace


void TransferProgress::fromJSON(
    const json_t *upload, const json_t *download, TransferProgress& transfer_progress)
{
    getTransferringListFromJSON(
        upload, UPLOAD, &transfer_progress.uploading_files);
    getTransferringListFromJSON(
        download, DOWNLOAD, &transfer_progress.downloading_files);
    getPendingFilesFromJSON(
        upload, &transfer_progress.n_pending_files);
    getTransferredListFromJSON(
        upload, UPLOAD, &transfer_progress.uploaded_files);
    getTransferredListFromJSON(
        download, DOWNLOAD, &transfer_progress.downloaded_files);
}
