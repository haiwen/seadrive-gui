#include "transfer-progress-dialog.h"

#include <QtGlobal>
#include <QtWidgets>
#include <QTabWidget>
#include <QTime>

#include "seadrive-gui.h"
#include "rpc/rpc-client.h"
#include "api/api-request.h"
#include "utils/utils.h"

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
const int kBytesColumnWidth = 100;
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

QString translateTransferRate(int rate)
{
    QString unit;
    QString display_rate;
    double KBps = ((double)rate) / 1024;
    if (KBps >= 1024) {
        unit = "MB/s";
        double MBps = KBps / 1024;
        if (MBps < 10) {
            display_rate = QString::number(MBps, 'f', 1);
        } else {
            display_rate = QString::number(int(MBps));
        }
    }
    else {
        display_rate = KBps;
        unit = "kB/s";
        display_rate = QString::number(int(KBps));
    }

    return QString("%1 %2")
        .arg(display_rate)
        .arg(unit);
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

    // QTabWidget* tab = new QTabWidget(this);
    // QWidget* upload_tab = new QWidget;
    // QWidget* download_tab = new QWidget;
    // tab->addTab(upload_tab, tr("Upload Progress"));
    // tab->addTab(download_tab, tr("Download Progress"));

    createTable();
    model_->setTransferItems();

    vlayout->insertWidget(1, table_);
    setLayout(vlayout);

    adjustSize();
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
    setDefaultAlignment(Qt::AlignLeft);
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

// QSize TransferItemsHeadView::sectionSizeFromContents(int index) const
// {
//     QSize size = QHeaderView::sectionSizeFromContents(index);
//     size.setWidth(index == FILE_COLUMN_PATH ? kNameColumnWidth
//                                      : kBytesColumnWidth);
//     return size;
// }

TransferItemsTableView::TransferItemsTableView(QWidget* parent)
    : QTableView(parent), source_model_(0)
{
    setHorizontalHeader(new TransferItemsHeadView(this));
    verticalHeader()->hide();

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);

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

TransferItemsTableModel::TransferItemsTableModel(QObject* parent)
    : QAbstractTableModel(parent),
      name_column_width_(kNameColumnWidth)
{
    uploading_files_.clear();
    uploaded_files_.clear();
    downloading_files_.clear();
    downloaded_files_.clear();
    progress_timer_ = new QTimer(this);
    connect(progress_timer_, SIGNAL(timeout()),
            this, SLOT(updateDownloadInfo()));
    progress_timer_->start(kRefreshProgressInterval);

}

void TransferItemsTableModel::setTransferItems()
{
    json_t *rpc_reply;
    json_error_t error;
    if (!gui->rpcClient()->getUploadProgress(&rpc_reply)) {
        return;
    }

    json_t* uploaded_array = json_object_get(rpc_reply, "uploaded_files");
    json_t* uploading_array = json_object_get(rpc_reply, "uploading_files");

    if (json_array_size(uploaded_array)) {
        int i, index, n = json_array_size(uploaded_array);
        for (i = 0; i < n; i++) {
            const char* name = json_string_value(json_array_get(uploaded_array, i));
            if (name) {
                uploaded_files_.push_back(QString::fromUtf8(name));
                for (index = 0; index != uploading_files_.size() ; index++) {
                    if (uploading_files_[index].file_path == name) {
                        uploading_files_.erase(uploading_files_.begin() + index);
                        break;
                    }
                }
            }
        }
    }

    beginResetModel();
    if (json_array_size(uploading_array)) {
        int i, index;
        json_t* uploading_object;

        json_array_foreach(uploading_array, i, uploading_object) {
            QMap<QString, QVariant> dict = mapFromJSON(uploading_object, &error);
            TransferInfo uploading_info;
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
    endResetModel();
}

int TransferItemsTableModel::columnCount(const QModelIndex& parent) const
{
    return FILE_MAX_COLUMN;
}

int TransferItemsTableModel::rowCount(const QModelIndex& parent) const
{
    return uploading_files_.size();
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

    uint row = index.row(), column = index.column();
    const TransferInfo& info = uploading_files_[row];

    if (role == Qt::SizeHintRole) {
        QSize qsize(0, kDefaultColumnHeight);
        if (column == FILE_COLUMN_PATH) {
            qsize.setWidth(name_column_width_);
        }
        else {
            qsize.setWidth(kBytesColumnWidth);
        }
        return qsize;
    }

    if (role == Qt::DisplayRole) {
        if (!uploading_files_.isEmpty()) {
            if (column == FILE_COLUMN_PATH) {
                QString path = info.file_path;
                return path.replace('\\', '/');
            }
            else if (column == FILE_COLUMN_SPEED) {
                if (info.last_second_bytes != 0) {
                    return translateTransferRate(info.transferred_bytes -
                                                 info.last_second_bytes);
                }
            }
            else if (column == FILE_COLUMN_PROGRESS) {
                return readableFileSize(info.transferred_bytes);
            }
            else if (column == FILE_COLUMN_SIZE) {
                return readableFileSize(info.total_bytes);
            }
        }
    }

    if (role == Qt::ToolTipRole) {
        if (!uploading_files_.isEmpty()) {
            if (column == FILE_COLUMN_PROGRESS) {
                uint speed = info.transferred_bytes -
                             info.last_second_bytes + 1;
                uint remain_bytes = info.total_bytes -
                                    info.transferred_bytes;
                QTime remain_time(0, 0, 0, 0);
                remain_time = remain_time.addSecs(remain_bytes / speed);
                return QString(tr("remain time %1"))
                               .arg(remain_time.toString("h:m:s"));
            }
            else {
                return  uploading_files_[row].file_path;
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
            return tr("Path");
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

const TransferInfo* TransferItemsTableModel::itemAt(int row) const
{
    if (row >= uploading_files_.size())
        return NULL;

    return &uploading_files_[row];
}

void TransferItemsTableModel::onResize(const QSize& size)
{
    name_column_width_ = size.width() - kBytesColumnWidth * (FILE_MAX_COLUMN - 1);
    if (rowCount() == 0)
        return;
    emit dataChanged(index(0, FILE_COLUMN_PATH),
                     index(rowCount()-1 , FILE_COLUMN_PATH));
}

void TransferItemsTableModel::updateDownloadInfo()
{
    setTransferItems();

    if (rowCount() == 0)
        return;
    emit dataChanged(index(0, FILE_COLUMN_PROGRESS),
                     index(rowCount()-1, FILE_COLUMN_PROGRESS));
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
    if (option.state || QStyle::State_Selected) {
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
    }
    case FILE_COLUMN_SPEED:
    {
    }
    case FILE_COLUMN_SIZE:
    {
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
    case FILE_COLUMN_PROGRESS:
    {
        const TransferInfo* transfer_item = model->itemAt(index.row());

        if (transfer_item != NULL) {
            const int progress = transfer_item->transferred_bytes * 100 /
                                 transfer_item->total_bytes;
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
