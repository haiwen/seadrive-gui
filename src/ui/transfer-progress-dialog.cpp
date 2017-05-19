#include "transfer-progress-dialog.h"

#include <QtGlobal>
#include <QtWidgets>
#include <QTime>
#include <QScopedPointer>

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

// Used with QScopedPointer for json_t
struct JsonPointerCustomDeleter {
    static inline void cleanup(json_t *json) {
        json_decref(json);
    }
};

} // namespace

TransferProgressDialog::TransferProgressDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Transfer Progress"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) |
                   Qt::WindowStaysOnTopHint);

    setMinimumSize(QSize(500, 200));

    QVBoxLayout* vlayout = new QVBoxLayout;

    tab_widget_ = new QTabWidget;
    tab_widget_->addTab(new TransferTab(UPLOAD), tr("upload"));
    tab_widget_->addTab(new TransferTab(DOWNLOAD), tr("download"));
    vlayout->addWidget(tab_widget_);

    setLayout(vlayout);
    adjustSize();
}

void TransferProgressDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    uint tab_width = (rect().width() / tab_widget_->count()) - 12;
    QString style("QTabBar::tab { width: %1px; }");
    style = style.arg(tab_width);
    tab_widget_->setStyleSheet(style);
}


TransferTab::TransferTab(TransferType type, QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout* vlayout = new QVBoxLayout;
    createTable(type);
    vlayout->addWidget(table_);
    vlayout->addStretch(1);
    setLayout(vlayout);
    adjustSize();
}

void TransferTab::createTable(TransferType type)
{
    table_ = new TransferItemsTableView(this);
    model_ = new TransferItemsTableModel(this);
    table_->setModel(model_);
    model_->setTransferItems();
    model_->setTransferType(type);
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
    progress_timer_ = new QTimer(this);
    connect(progress_timer_, SIGNAL(timeout()),
            this, SLOT(updateTransferringInfo()));
    progress_timer_->start(kRefreshProgressInterval);

}

void TransferItemsTableModel::setTransferItems()
{
    json_t *upload_reply, *download_reply;

    if ((!gui->rpcClient()->getUploadProgress(&upload_reply)) ||
        (!gui->rpcClient()->getDownloadProgress(&download_reply))) {
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> upload(upload_reply);
    QScopedPointer<json_t, JsonPointerCustomDeleter> download(download_reply);

    beginResetModel();
    transfer_progress_ =
        TransferProgress::fromJSON(upload.data(), download.data());
    endResetModel();
}

int TransferItemsTableModel::columnCount(const QModelIndex& parent) const
{
    return FILE_MAX_COLUMN;
}

int TransferItemsTableModel::rowCount(const QModelIndex& parent) const
{
    if (transfer_type_ == UPLOAD) {
        return transfer_progress_.uploading_files_.size() +
            transfer_progress_.uploaded_files_.size();
    } else {
        return transfer_progress_.downloading_files_.size() +
            transfer_progress_.downloaded_files_.size();
    }
}

QVariant TransferItemsTableModel::transferringData(
    const QModelIndex& index, int role) const
{
    if (role != Qt::DisplayRole && role != Qt::ToolTipRole) {
        return QVariant();
    } else if (isTransferringRow(index) == false) {
        return QVariant();
    } else {
        const uint column = index.column();
        const uint transferring_index = index.row();
        const TransferringInfo * transferring_info;

        if (transfer_type_ == UPLOAD) {
            if (transfer_progress_.uploading_files_.isEmpty()) {
                return QVariant();
            }
            transferring_info = &transfer_progress_.uploading_files_[transferring_index];
        } else if (transfer_type_ == DOWNLOAD) {
            if (transfer_progress_.downloading_files_.isEmpty()) {
                return QVariant();
            }
            transferring_info = &transfer_progress_.downloading_files_[transferring_index];
        }

        if (role == Qt::DisplayRole) {
            if (column == FILE_COLUMN_PATH) {
                return getBaseName(transferring_info->file_path);
            }
            else if (column == FILE_COLUMN_PROGRESS) {
                return readableFileSize(transferring_info->transferred_bytes);
            }
            else if (column == FILE_COLUMN_SIZE) {
                return readableFileSize(transferring_info->total_bytes);
            }
        } else if (role == Qt::ToolTipRole) {
            return normalizedPath(transferring_info->file_path);
        }
    }

    return QVariant();
}

QVariant TransferItemsTableModel::transferredData(
    const QModelIndex& index, int role) const
{
    if (role != Qt::DisplayRole && role != Qt::ToolTipRole) {
        return QVariant();
    } else if (isTransferringRow(index)) {
        return QVariant();
    } else {
        const uint column = index.column();
        const TransferredInfo * transferred_info;
        uint transferred_index;

        if (transfer_type_ == UPLOAD) {
            if (transfer_progress_.uploaded_files_.isEmpty()) {
                return QVariant();
            }
            transferred_index = index.row() - transfer_progress_.uploading_files_.size();
            transferred_info = &transfer_progress_.uploaded_files_[transferred_index];
        } else if (transfer_type_ == DOWNLOAD) {
            if (transfer_progress_.downloaded_files_.isEmpty()) {
                return QVariant();
            }
            transferred_index = index.row() - transfer_progress_.downloading_files_.size();
            transferred_info = &transfer_progress_.downloaded_files_[transferred_index];
        }

        if (role == Qt::DisplayRole) {
            if (column == FILE_COLUMN_PATH) {
                return getBaseName(transferred_info->file_path);
            }
            else if (column == FILE_COLUMN_PROGRESS) {
                return QString(tr("finished"));
            }
            else if (column == FILE_COLUMN_SIZE) {
                return QVariant();
            }
        } else if (role == Qt::ToolTipRole) {
            return normalizedPath(transferred_info->file_path);
        }
    }

    return QVariant();
}

QVariant TransferItemsTableModel::data(
    const QModelIndex& index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role != Qt::DisplayRole &&
        role != Qt::SizeHintRole &&
        role != Qt::ToolTipRole) {
        return QVariant();
    }

    const uint column = index.column();

    if (role == Qt::SizeHintRole) {
        QSize qsize(0, kDefaultColumnHeight);
        if (column == FILE_COLUMN_PATH) {
            qsize.setWidth(name_column_width_);
        } else {
            qsize.setWidth(kDefaultColumnWidth);
        }
        return qsize;
    }

    if (isTransferringRow(index)) {
        return transferringData(index, role);
    } else {
        return transferredData(index, role);
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
        else if (section == FILE_COLUMN_PROGRESS) {
            return tr("Progress");
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
        if (row >= transfer_progress_.downloading_files_.size()) {
            return NULL;
        } else {
            return &transfer_progress_.downloading_files_[row];
        }
    }
    else {
        if (row >= transfer_progress_.uploading_files_.size()) {
            return NULL;
        } else {
            return &transfer_progress_.uploading_files_[row];
        }
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

bool TransferItemsTableModel::isTransferringRow(
    const QModelIndex& index) const
{
    uint row = index.row();
    uint transferring_size = 0;
    if (transfer_type_ == UPLOAD) {
        transferring_size = transfer_progress_.uploading_files_.size();
    } else {
        transferring_size = transfer_progress_.downloading_files_.size();
    }
    return row < transferring_size;
}

void TransferItemsTableModel::setTransferType(TransferType type)
{
    transfer_type_ = type;
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
        if (!model->isTransferringRow(index)) {
            QPoint text_pos(kMarginLeft, kMarginTop);
            text_pos += option_rect.topLeft();

            QRect text_rect(text_pos, size);
            painter->save();
            painter->setPen(kItemColor);
            painter->drawText(text_rect,
                              Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                              text, &text_rect);
            painter->restore();
        } else {
            const TransferringInfo* transferring_item =
                model->itemAt(index.row());

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
