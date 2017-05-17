#include "transfer-progress-dialog.h"

#include <QtGlobal>
#include <QtWidgets>
#include <QTabWidget>
#include <QTime>

#include "seadrive-gui.h"
#include "rpc/rpc-client.h"
#include "api/api-request.h"
#include "utils/utils.h"
#include "utils/paint-utils.h"
#include "utils/file-utils.h"

namespace
{
enum {
    FILE_COLUMN_PATH = 0,
    FILE_COLUMN_SPEED,
    FILE_COLUMN_PROGRESS,
    FILE_COLUMN_SIZE,
    FILE_MAX_COLUMN,
};

const int kNameColumnWidth = 300;
const int kDefaultColumnWidth = 100;
const int kDefaultColumnHeight = 40;

const int kMarginLeft = 5;
const int kMarginTop = -5;
const int kPadding = 5;

const int kRefreshProgressInterval = 1000;

const QColor kSelectedItemBackgroundcColor("#F9E0C7");
const QColor kItemBackgroundColor("white");
const QColor kItemBottomBorderColor("#f3f3f3");
const QColor kItemColor("black");
const QString kProgressBarStyle("QProgressBar "
        "{ border: 1px solid grey; border-radius: 2px; } "
        "QProgressBar::chunk { background-color: #f0f0f0; width: 20px; }");

QString normalizedPath(const QString& file_path)
{
    QString normalized_path = file_path;
    return normalized_path.replace('\\', '/');
}

uint getFileSize(const QString& file_path)
{
    QString fill_path = pathJoin(gui->mountDir(), file_path);
    QFile file(fill_path);
    if (file.exists()) {
        return file.size();
    }
    return 0;
}

} // namespace

TransferProgressDialog::TransferProgressDialog(QWidget *parent)
{
    setWindowTitle(tr("Transfer Progress"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) |
                   Qt::WindowStaysOnTopHint);

    setMinimumSize(QSize(600, 300));

    QVBoxLayout* vlayout = new QVBoxLayout;

    QTabBar* tab = new QTabBar(this);
    tab->addTab(tr("upload"));
    tab->addTab(tr("download"));
    vlayout->addWidget(tab);

    createTable();
    model_->setTransferItems();
    vlayout->addWidget(table_);

    setLayout(vlayout);
    adjustSize();

    connect(tab, SIGNAL(currentChanged(int)),
            model_, SLOT(setTransferType(int)));
}

void TransferProgressDialog::createTable()
{
    table_ = new TransferItemsTableView(this);
    model_ = new TransferItemsTableModel(this);
    table_->setModel(model_);

}


TransferItemsHeadView::TransferItemsHeadView(QWidget* parent)
    : QHeaderView(Qt::Horizontal, parent)
{
    setStretchLastSection(false);
    setCascadingSectionResizes(true);
    setHighlightSections(false);
    setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    setSectionResizeMode(QHeaderView::ResizeToContents);
#else
    setResizeMode(QHeaderView::ResizeToContents);
#endif
}

QSize TransferItemsHeadView::sectionSizeFromContents(int index) const
{
    QSize size = QHeaderView::sectionSizeFromContents(index);
    TransferItemsTableView* table = (TransferItemsTableView*)parent();
    TransferItemsTableModel* model =
        (TransferItemsTableModel*)(table->sourceModel());

    if (model) {
        size.setWidth(index == FILE_COLUMN_PATH ? model->nameColumnWidth()
                                                : kDefaultColumnWidth);
    }

    return size;
}


TransferItemsTableView::TransferItemsTableView(QWidget* parent)
    : QTableView(parent), source_model_(0)
{
    setHorizontalHeader(new TransferItemsHeadView(this));
    verticalHeader()->hide();

    setSelectionBehavior(QAbstractItemView::SelectRows);
    // setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionMode(QAbstractItemView::NoSelection);

    setMouseTracking(true);
    setShowGrid(false);
    setContentsMargins(0, 5, 0, 5);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    horizontalScrollBar()->close();

    setItemDelegate(new TransferItemDelegate(this));
}

void TransferItemsTableView::resizeEvent(QResizeEvent* event)
{
    QTableView::resizeEvent(event);
    if (source_model_)
        source_model_->onResize(event->size());
}

void TransferItemsTableView::setModel(QAbstractItemModel* model)
{
    QTableView::setModel(model);
    source_model_ = qobject_cast<TransferItemsTableModel*>(model);
}

TransferItemsTableModel* TransferItemsTableView::sourceModel()
{
    return source_model_;
}


TransferItemsTableModel::TransferItemsTableModel(QObject* parent)
    : QAbstractTableModel(parent),
      name_column_width_(kNameColumnWidth),
      transfer_type_(UPLOAD)
{
    uploading_files_.clear();
    uploaded_files_.clear();
    downloading_files_.clear();
    downloaded_files_.clear();
    progress_timer_ = new QTimer(this);
    connect(progress_timer_, SIGNAL(timeout()),
            this, SLOT(updateTransferringInfo()));
    progress_timer_->start(kRefreshProgressInterval);

}

void TransferItemsTableModel::setTransferItems()
{
    json_t *upload_reply, *download_reply;
    json_error_t error;

    if (!gui->rpcClient()->getUploadProgress(&upload_reply)) {
        return;
    }
    if (!gui->rpcClient()->getDownloadProgress(&download_reply)) {
        return;
    }

    json_t* uploaded_array = json_object_get(upload_reply, "uploaded_files");
    json_t* uploading_array = json_object_get(upload_reply, "uploading_files");
    json_t* downloaded_array = json_object_get(download_reply, "downloaded_files");
    json_t* downloading_array = json_object_get(download_reply, "downloading_files");

    beginResetModel();
    if (json_array_size(uploaded_array)) {
        int i, n = json_array_size(uploaded_array);
        int uploading_files_index, uploaded_files_index;

        for (i = 0; i < n; i++) {
            const char* name = json_string_value(json_array_get(uploaded_array, i));
            if (name) {
                TransferredInfo uploaded_info;
                uploaded_info.file_path = QString::fromUtf8(name);
                uploaded_info.total_bytes = getFileSize(name);

                for (uploaded_files_index = 0; uploaded_files_index != uploaded_files_.size(); uploaded_files_index++) {
                    if (uploaded_files_[uploaded_files_index].file_path == uploaded_info.file_path) {
                        break;
                    }
                }
                if (uploaded_files_index == uploaded_files_.size()) {
                    uploaded_files_.push_back(uploaded_info);
                }

                for (uploading_files_index = 0; uploading_files_index != uploading_files_.size(); uploading_files_index++) {
                    if (uploading_files_[uploading_files_index].file_path == name) {
                        uploading_files_.erase(uploading_files_.begin() + uploading_files_index);
                        break;
                    }
                }
            }
        }
    }

    if (json_array_size(uploading_array)) {
        int i, index;
        json_t* uploading_object;

        json_array_foreach(uploading_array, i, uploading_object) {
            QMap<QString, QVariant> dict = mapFromJSON(uploading_object, &error);
            TransferringInfo uploading_info;
            uploading_info.file_path = dict.value("file_path").toString();
            uploading_info.last_second_bytes = 0;
            uploading_info.transferred_bytes = dict.value("uploaded").toUInt();
            uploading_info.total_bytes = dict.value("total_upload").toUInt();

            for (index = 0; index != uploading_files_.size() ; index++) {
                if (uploading_files_[index].file_path == uploading_info.file_path) {
                    uploading_files_[index].last_second_bytes = uploading_files_[index].transferred_bytes;
                    uploading_files_[index].transferred_bytes = uploading_info.transferred_bytes;
                    break;
                }
            }

            if (index == uploading_files_.size()) {
                uploading_files_.push_back(uploading_info);
            }
        }
    }

    if (json_array_size(downloaded_array)) {
        int i, n = json_array_size(downloaded_array);
        int downloading_files_index, downloaded_files_index;

        for (i = 0; i < n; i++) {
            const char* name = json_string_value(json_array_get(downloaded_array, i));
            if (name) {
                TransferredInfo downloaded_info;
                downloaded_info.file_path = QString::fromUtf8(name);
                downloaded_info.total_bytes = getFileSize(name);

                for (downloaded_files_index = 0; downloaded_files_index != downloaded_files_.size(); downloaded_files_index++) {
                    if (downloaded_files_[downloaded_files_index].file_path == downloaded_info.file_path) {
                        break;
                    }
                }
                if (downloaded_files_index == downloaded_files_.size()) {
                    downloaded_files_.push_back(downloaded_info);
                }

                for (downloading_files_index = 0; downloading_files_index != downloading_files_.size(); downloading_files_index++) {
                    if (downloading_files_[downloading_files_index].file_path == name) {
                        downloading_files_.erase(downloading_files_.begin() + downloading_files_index);
                        break;
                    }
                }
            }
        }
    }

    if (json_array_size(downloading_array)) {
        int i, index;
        json_t* downloading_object;

        json_array_foreach(downloading_array, i, downloading_object) {
            QMap<QString, QVariant> dict = mapFromJSON(downloading_object, &error);
            TransferringInfo downloading_info;
            downloading_info.file_path = dict.value("file_path").toString();
            downloading_info.last_second_bytes = 0;
            downloading_info.transferred_bytes = dict.value("downloaded").toUInt();
            downloading_info.total_bytes = dict.value("total_download").toUInt();

            for (index = 0; index != downloading_files_.size() ; index++) {
                if (downloading_files_[index].file_path == downloading_info.file_path) {
                    downloading_files_[index].last_second_bytes = downloading_files_[index].transferred_bytes;
                    downloading_files_[index].transferred_bytes = downloading_info.transferred_bytes;
                    break;
                }
            }

            if (index == downloading_files_.size()) {
                downloading_files_.push_back(downloading_info);
            }
        }
    }
    endResetModel();
}

int TransferItemsTableModel::columnCount(const QModelIndex& parent) const
{
    return FILE_MAX_COLUMN;
}

int TransferItemsTableModel::rowCount(const QModelIndex& parent) const
{
    if (transfer_type_ == UPLOAD) {
        return uploading_files_.size() +
               uploaded_files_.size();
    }
    return downloading_files_.size() +
           downloaded_files_.size();
}

QVariant TransferItemsTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role != Qt::DisplayRole &&
        role != Qt::SizeHintRole &&
        role != Qt::ToolTipRole) {
        return QVariant();
    }

    const int row = index.row(), column = index.column();

    if (role == Qt::SizeHintRole) {
        QSize qsize(0, kDefaultColumnHeight);
        if (column == FILE_COLUMN_PATH) {
            qsize.setWidth(name_column_width_);
        }
        else {
            qsize.setWidth(kDefaultColumnWidth);
        }
        return qsize;
    }

    if (transfer_type_ == UPLOAD) {
        if (row >= uploading_files_.size() &&
            uploaded_files_.size() > 0)
        {
            const int transferred_index = row - uploading_files_.size();
            const TransferredInfo& transferred_info = uploaded_files_[transferred_index];

            if (role == Qt::DisplayRole) {
                if (column == FILE_COLUMN_PATH) {
                    return getBaseName(transferred_info.file_path);
                }
                else if (column == FILE_COLUMN_SPEED) {
                    return QVariant();
                }
                else if (column == FILE_COLUMN_PROGRESS) {
                    return QString(tr("finished"));
                }
                else if (column == FILE_COLUMN_SIZE) {
                    return readableFileSize(transferred_info.total_bytes);
                }
            }

            if (role == Qt::ToolTipRole) {
                return normalizedPath(transferred_info.file_path);
            }
        }

        const TransferringInfo& transferring_info = uploading_files_[row];

        if (!uploading_files_.isEmpty()) {
            if (role == Qt::DisplayRole) {
                if (column == FILE_COLUMN_PATH) {
                    return getBaseName(transferring_info.file_path);
                }
                else if (column == FILE_COLUMN_SPEED) {
                    if (transferring_info.last_second_bytes != 0) {
                        return translateTransferRate(transferring_info.transferred_bytes -
                                                     transferring_info.last_second_bytes);
                    }
                }
                else if (column == FILE_COLUMN_PROGRESS) {
                    return readableFileSize(transferring_info.transferred_bytes);
                }
                else if (column == FILE_COLUMN_SIZE) {
                    return readableFileSize(transferring_info.total_bytes);
                }
            }

            if (role == Qt::ToolTipRole) {
                if (column == FILE_COLUMN_PROGRESS) {
                    uint speed = transferring_info.transferred_bytes -
                                 transferring_info.last_second_bytes + 1;
                    uint remain_bytes = transferring_info.total_bytes -
                                        transferring_info.transferred_bytes;
                    QTime remain_time(0, 0, 0, 0);
                    remain_time = remain_time.addSecs(remain_bytes / speed);
                    return QString(tr("remain time %1"))
                                   .arg(remain_time.toString("h:m:s"));
                }
                else {
                    return normalizedPath(transferring_info.file_path);
                }
            }
        }
    } // if (transfer_type_ == UPLOAD)
    else {
        if (row >= downloading_files_.size() &&
            downloaded_files_.size() > 0)
        {
            const int transferred_index = row - downloading_files_.size();
            const TransferredInfo& transferred_info = downloaded_files_[transferred_index];

            if (role == Qt::DisplayRole) {
                if (column == FILE_COLUMN_PATH) {
                    return getBaseName(transferred_info.file_path);
                }
                else if (column == FILE_COLUMN_SPEED) {
                    return QVariant();
                }
                else if (column == FILE_COLUMN_PROGRESS) {
                    return QString(tr("finished"));
                }
                else if (column == FILE_COLUMN_SIZE) {
                    return readableFileSize(transferred_info.total_bytes);
                }
            }

            if (role == Qt::ToolTipRole) {
                return normalizedPath(transferred_info.file_path);
            }
        }

        const TransferringInfo& transferring_info = downloading_files_[row];

        if (!downloading_files_.isEmpty()) {
            if (role == Qt::DisplayRole) {
                if (column == FILE_COLUMN_PATH) {
                    return getBaseName(transferring_info.file_path);
                }
                else if (column == FILE_COLUMN_SPEED) {
                    if (transferring_info.last_second_bytes != 0) {
                        return translateTransferRate(transferring_info.transferred_bytes -
                                                     transferring_info.last_second_bytes);
                    }
                }
                else if (column == FILE_COLUMN_PROGRESS) {
                    return readableFileSize(transferring_info.transferred_bytes);
                }
                else if (column == FILE_COLUMN_SIZE) {
                    return readableFileSize(transferring_info.total_bytes);
                }
            }

            if (role == Qt::ToolTipRole) {
                if (column == FILE_COLUMN_PROGRESS) {
                    uint speed = transferring_info.transferred_bytes -
                                 transferring_info.last_second_bytes + 1;
                    uint remain_bytes = transferring_info.total_bytes -
                                        transferring_info.transferred_bytes;
                    QTime remain_time(0, 0, 0, 0);
                    remain_time = remain_time.addSecs(remain_bytes / speed);
                    return QString(tr("remain time %1"))
                                   .arg(remain_time.toString("h:m:s"));
                }
                else {
                    return normalizedPath(transferring_info.file_path);
                }
            }
        }
    }

    return QVariant();
}

QVariant TransferItemsTableModel::headerData(int section,
                                             Qt::Orientation orientation,
                                             int role) const
{
    if (orientation == Qt::Vertical) {
        return QVariant();
    }

    if (role == Qt::DisplayRole) {
        if (section == FILE_COLUMN_PATH) {
            return tr("Name");
        }
        else if (section == FILE_COLUMN_SPEED) {
            return tr("Speed");
        }
        else if (section == FILE_COLUMN_PROGRESS) {
            return tr("Transferred");
        }
        else if (section == FILE_COLUMN_SIZE) {
            return tr("Size");
        }
    }

    return QVariant();
}

const TransferringInfo* TransferItemsTableModel::itemAt(int row) const
{
    if (transfer_type_ == DOWNLOAD) {
        if (row >= downloading_files_.size()) {
            return NULL;
        }
        return &downloading_files_[row];
    }
    else {
        if (row >= uploading_files_.size()) {
            return NULL;
        }
        return &uploading_files_[row];
    }
}

void TransferItemsTableModel::onResize(const QSize& size)
{
    name_column_width_ = size.width() - kDefaultColumnWidth * (FILE_MAX_COLUMN - 1);
    if (rowCount() == 0)
        return;
    emit dataChanged(index(0, FILE_COLUMN_PATH),
                     index(rowCount()-1 , FILE_COLUMN_PATH));
}

void TransferItemsTableModel::updateTransferringInfo()
{
    setTransferItems();

    if (rowCount() == 0)
        return;
    emit dataChanged(index(0, FILE_COLUMN_PROGRESS),
                     index(rowCount()-1, FILE_COLUMN_PROGRESS));
}

uint TransferItemsTableModel::nameColumnWidth() const
{
    return name_column_width_;
}

void TransferItemsTableModel::setTransferType(const int transfer_type)
{
    if (transfer_type == 1) {
        transfer_type_ = DOWNLOAD;
    }
    else {
         transfer_type_ = UPLOAD;
    }
}


TransferItemDelegate::TransferItemDelegate(QObject *parent)
   : QStyledItemDelegate(parent)
{
}

void TransferItemDelegate::paint(QPainter *painter,
                                 const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    const TransferItemsTableModel* model =
        static_cast<const TransferItemsTableModel*>(index.model());

    QRect option_rect = option.rect;

    painter->save();
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option_rect, kSelectedItemBackgroundcColor);
    }
    else
        painter->fillRect(option_rect, kItemBackgroundColor);
    painter->restore();

    QSize size = model->data(index, Qt::SizeHintRole).value<QSize>();
    QString text = model->data(index, Qt::DisplayRole).value<QString>();

    switch (index.column()) {
    case FILE_COLUMN_PATH:
    {
        QPoint text_pos(kMarginLeft, kMarginTop);
        text_pos += option_rect.topLeft();
        QRect text_rect(text_pos, size);
        QFont font = model->data(index, Qt::FontRole).value<QFont>();

        painter->save();
        painter->setPen(kItemColor);
        painter->setFont(font);
        painter->drawText(text_rect,
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                          fitTextToWidth(text, option.font, text_rect.width() - 20));
        painter->restore();

        break;
    }
    case FILE_COLUMN_SPEED:
    {
    }
    case FILE_COLUMN_SIZE:
    {
        QPoint text_pos(kMarginLeft, kMarginTop);
        text_pos += option_rect.topLeft();
        QRect text_rect(text_pos, size);
        QFont font = model->data(index, Qt::FontRole).value<QFont>();

        painter->save();
        painter->setPen(kItemColor);
        painter->setFont(font);
        painter->drawText(text_rect,
                          Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                          text, &text_rect);
        painter->restore();

        break;
    }
    case FILE_COLUMN_PROGRESS:
    {
        if (text == tr("finished")) {
            QPoint text_pos(kMarginLeft, kMarginTop);
            text_pos += option_rect.topLeft();

            QRect text_rect(text_pos, size);
            painter->save();
            painter->setPen(kItemColor);
            painter->drawText(text_rect,
                              Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                              text, &text_rect);
            painter->restore();

            break;
        }

        const TransferringInfo* transferring_item = model->itemAt(index.row());

        if (transferring_item != NULL) {
            const int progress = transferring_item->transferred_bytes * 100 /
                                 transferring_item->total_bytes;
            // Customize style using style-sheet..
            QProgressBar progressBar;
            progressBar.resize(QSize(size.width() - 10, size.height() / 2 - 4));
            progressBar.setMinimum(0);
            progressBar.setMaximum(100);
            progressBar.setValue(progress);
            progressBar.setAlignment(Qt::AlignCenter);
            progressBar.setStyleSheet(kProgressBarStyle);
            painter->save();
            painter->translate(option_rect.topLeft() + QPoint(kPadding, size.height() / 4 - 2));
            progressBar.render(painter);
            painter->restore();
        }
        break;
    }
    default:
    {
        qWarning() << "invalid item (row)";
        break;
    }
    }
}
