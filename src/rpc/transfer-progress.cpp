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
        transferring_info.transferred_bytes =
            dict.value(transferred_name.toUtf8().data()).toULongLong();
        transferring_info.total_bytes =
            dict.value(total_bytes_name.toUtf8().data()).toULongLong();
        list->push_back(transferring_info);
    }
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

    int array_size = json_array_size(transferred_array);
    for (int i = 0; i < array_size; i++) {
        const char* name = json_string_value(
            json_array_get(transferred_array, i));
        TransferredInfo transferred_info;
        transferred_info.file_path = QString::fromUtf8(name);
        list->push_back(transferred_info);
    }
}

} // namespace


TransferProgress TransferProgress::fromJSON(
    const json_t *upload, const json_t *download)
{
    TransferProgress transfer_progress;

    getTransferringListFromJSON(
        upload, UPLOAD, &transfer_progress.uploading_files);
    getTransferringListFromJSON(
        download, DOWNLOAD, &transfer_progress.downloading_files);
    getTransferredListFromJSON(
        upload, UPLOAD, &transfer_progress.uploaded_files);
    getTransferredListFromJSON(
        download, DOWNLOAD, &transfer_progress.downloaded_files);

    return transfer_progress;
}
