#include "search-dialog.h"

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
    FILE_COLUMN_NAME = 0,
    FILE_COLUMN_MTIME,
    FILE_COLUMN_SIZE,
    FILE_COLUMN_KIND,
    FILE_MAX_COLUMN
};

enum {
    INDEX_LOADING_FAILED_VIEW = 5,
    INDEX_EMPTY_VIEW,
    INDEX_SEARCH_VIEW
};


const int kNameColumnWidth = 300;
const int kDefaultColumnWidth = 100;
const int kDefaultColumnHeight = 40;
const char *kLoadingFailedLabelName = "LoadingFailedText";
const int kToolBarIconSize = 24;
const int kStatusBarIconSize = 20;

const int kAllPage = 1;
const int kPerPageCount = 10000;
//zhe.................
const int kColumnIconSize = 28;
const int kFileNameColumnWidth = 200;
const int kExtraPadding = 30;
const int kDefaultColumnSum = kFileNameColumnWidth + kDefaultColumnWidth * 3 + kExtraPadding;
const int kFileStatusIconSize = 16;
const int kMarginBetweenFileNameAndStatusIcon = 5;

const QColor kFileNameFontColor("black");
const QColor kFontColor("#757575");
//zhe..............
const int kMarginLeft = 5;
const int kMarginTop = -5;
const int kPadding = 5;

const int kRefreshProgressInterval = 1000;

const QColor kSelectedItemBackgroundcColor("#F9E0C7");
const QColor kItemBackgroundColor("white");
const QColor kItemBottomBorderColor("#f3f3f3");
const QColor kItemColor("black");

} // namespace

SearchDialog::SearchDialog(const Account &account, QWidget *parent)
    : QDialog(parent),
      account_(account),
      search_request_(NULL),
      search_text_last_modified_(0)
{
    setWindowTitle(tr("Search Progress"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) |
                   Qt::WindowStaysOnTopHint);

    setMinimumSize(QSize(600, 371));
    createToolBar();
    createLoadingFailedView();
    createEmptyView();
    createTable();

    QWidget* widget = new QWidget;
    widget->setObjectName("mainWidget");
    QVBoxLayout* layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    setLayout(layout);
    layout->addWidget(widget);

    QVBoxLayout *vlayout = new QVBoxLayout;
    vlayout->setContentsMargins(1, 0, 1, 0);
    vlayout->setSpacing(0);
    widget->setLayout(vlayout);

    QHBoxLayout *hlayout = new QHBoxLayout;
    hlayout->setContentsMargins(1, 0, 1, 0);
    hlayout->setSpacing(0);
    hlayout->addWidget(toolbar_);

    stack_ = new QStackedWidget;
    stack_->insertWidget(INDEX_SEARCH_VIEW, search_view_);
    stack_->insertWidget(INDEX_EMPTY_VIEW, empty_view_);
    stack_->insertWidget(INDEX_LOADING_FAILED_VIEW, loading_failed_view_);
    stack_->setContentsMargins(0, 0, 0, 0);
    stack_->installEventFilter(this);
    stack_->setAcceptDrops(true);

    vlayout->addLayout(hlayout);
    vlayout->addWidget(stack_);

    search_timer_ = new QTimer(this);
    connect(search_timer_, SIGNAL(timeout()), this, SLOT(doRealSearch()));
    search_timer_->start(300);

    connect(search_view_, SIGNAL(clearSearchBar()),
            search_bar_, SLOT(clear()));
}

SearchDialog::~SearchDialog()
{
    if (search_request_ != NULL)
        search_request_->deleteLater();
}


void SearchDialog::createToolBar()
{
    toolbar_ = new QToolBar;
    toolbar_->setObjectName("toolBar");
    toolbar_->setIconSize(QSize(kToolBarIconSize, kToolBarIconSize));
    toolbar_->setStyleSheet("QToolbar { spacing: 0px; }");

    search_bar_ = new SearchBar;
    search_bar_->setPlaceholderText(tr("Search files"));
    search_bar_->setFixedWidth(250);
    toolbar_->addWidget(search_bar_);
    connect(search_bar_, SIGNAL(textChanged(const QString&)),
            this, SLOT(doSearch(const QString&)));

    refresh_button_ = new QToolButton;
    refresh_button_->setObjectName("refreshButton");
    refresh_button_->setToolTip(tr("Refresh"));
    refresh_button_->setIcon(QIcon(":/images/filebrowser/refresh-gray.png"));
    refresh_button_->setIconSize(QSize(kStatusBarIconSize, kStatusBarIconSize));
    refresh_button_->installEventFilter(this);
    connect(refresh_button_, SIGNAL(clicked()), this, SLOT(onRefresh()));
    toolbar_->addWidget(refresh_button_);
}

void SearchDialog::createLoadingFailedView()
{
    loading_failed_view_ = new QLabel;
    loading_failed_view_->setObjectName(kLoadingFailedLabelName);
    QString link = QString("<a style=\"color:#777\" href=\"#\">%1</a>").arg(tr("retry"));
    QString label_text = tr("Failed to get files information<br/>"
                            "Please %1").arg(link);
    loading_failed_view_->setText(label_text);
    loading_failed_view_->setAlignment(Qt::AlignCenter);

    connect(loading_failed_view_, SIGNAL(linkActivated(const QString&)),
            this, SLOT(onRefresh()));
}

void SearchDialog::createEmptyView()
{
    empty_view_ = new QLabel(this);
    empty_view_->setText(tr("This folder is empty."));
    empty_view_->setAlignment(Qt::AlignCenter);
    empty_view_->setStyleSheet("background-color: white;");
}

void SearchDialog::createTable()
{
    search_view_ = new SearchItemsTableView(this);
    search_view_->setObjectName("searchResult");
    search_model_ = new SearchItemsTableModel(this);
    search_view_->setModel(search_model_);
#ifdef Q_OS_MAC
    search_view_->setAttribute(Qt::WA_MacShowFocusRect, 0);
#endif
    search_delegate_ = new SearchItemsDelegate(this);
    delete search_view_->itemDelegate();
    search_view_->setItemDelegate(search_delegate_);
//    QDialog::resizeEvent(QResizeEvent *event);
}

bool SearchDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == refresh_button_) {
        if (event->type() == QEvent::Enter) {
            refresh_button_->setStyleSheet("FileBrowserDialog QToolButton#refreshButton {"
                                          "background: #DFDFDF; padding: 3px;"
                                          "margin-right: 12px; border-radius: 2px;}");
            return true;
        } else if (event->type() == QEvent::Leave) {
            refresh_button_->setStyleSheet("FileBrowserDialog QToolButton#refreshButton {"
                                          "background: #F5F5F7; padding: 0px;"
                                          "margin-right: 15px;}");
            return true;
        }
    } else if (obj == stack_) {
        if (stack_->currentIndex() == INDEX_EMPTY_VIEW ||
            stack_->currentIndex() == INDEX_SEARCH_VIEW) {
            return true;
        }
    }

    return QObject::eventFilter(obj, event);
}


void SearchDialog::doSearch(const QString &keyword)
{
    // make it search utf-8 charcters
    if (keyword.toUtf8().size() < 3) {
        return;
    }

    // save for doRealSearch
    search_text_last_modified_ = QDateTime::currentMSecsSinceEpoch();
}

void SearchDialog::doRealSearch()
{
    // not modified
    if (search_text_last_modified_ == 0)
        return;
    // modified too fast
    if (QDateTime::currentMSecsSinceEpoch() - search_text_last_modified_ <= 300)
        return;

    if (!account_.isValid())
        return;

    if (search_request_) {
        // search_request_->abort();
        search_request_->deleteLater();
        search_request_ = NULL;
    }

//    stack_->setCurrentIndex(INDEX_LOADING_VIEW);

    search_request_ = new FileSearchRequest(account_, search_bar_->text(), kAllPage, kPerPageCount);
    connect(search_request_, SIGNAL(success(const std::vector<FileSearchResult>&, bool, bool)),
            this, SLOT(onSearchSuccess(const std::vector<FileSearchResult>&, bool, bool)));
    connect(search_request_, SIGNAL(failed(const ApiError&)),
            this, SLOT(onSearchFailed(const ApiError&)));

    search_request_->send();

    // reset
    search_text_last_modified_ = 0;
}

void SearchDialog::onSearchSuccess(const std::vector<FileSearchResult>& results,
                                bool is_loading_more,
                                bool has_more)
{
    search_model_->setSearchResult(results);
    stack_->setCurrentIndex(INDEX_SEARCH_VIEW);
//    updateFileCount();
}

void SearchDialog::onSearchFailed(const ApiError& error)
{
    stack_->setCurrentIndex(INDEX_LOADING_FAILED_VIEW);
}


SearchItemsTableView::SearchItemsTableView(QWidget* parent)
    : QTableView(parent),
      parent_(qobject_cast<SearchDialog*>(parent)),
      search_model_(NULL),
      proxy_model_(NULL)
{
    verticalHeader()->hide();
    setSelectionBehavior(QAbstractItemView::SelectRows);
    // setSelectionMode(QAbstractItemView::ExtendedSelection);
    setSelectionMode(QAbstractItemView::NoSelection);
    setMouseTracking(true);
    setShowGrid(false);
    setContentsMargins(0, 5, 0, 5);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    horizontalScrollBar()->close();
    setItemDelegate(new SearchItemsDelegate(this));

//    verticalHeader()->hide();
//    verticalHeader()->setDefaultSectionSize(kDefaultColumnHeight);
//    horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
//    horizontalHeader()->setStretchLastSection(true);
//    horizontalHeader()->setCascadingSectionResizes(true);
//    horizontalHeader()->setHighlightSections(false);
//    horizontalHeader()->setSortIndicatorShown(true);
//    horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

//    setGridStyle(Qt::NoPen);
//    setShowGrid(false);
//    setContentsMargins(0, 0, 0, 0);
//    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

//    setSelectionBehavior(QAbstractItemView::SelectRows);
//    setSelectionMode(QAbstractItemView::ExtendedSelection);

//    setMouseTracking(true);
//    setDragDropMode(QAbstractItemView::DropOnly);

    connect(this, SIGNAL(doubleClicked(const QModelIndex&)),
                this, SLOT(onItemDoubleClick(const QModelIndex&)));
}

void SearchItemsTableView::resizeEvent(QResizeEvent* event)
{
    QTableView::resizeEvent(event);
    if (search_model_)
        search_model_->onResize(event->size());
}

void SearchItemsTableView::contextMenuEvent(QContextMenuEvent *event)
{
    QPoint position = event->pos();
    const QModelIndex proxy_index = indexAt(position);
    position = viewport()->mapToGlobal(position);
    if (!proxy_index.isValid()) {
        return;
    }

    const QModelIndex index = proxy_model_->mapToSource(proxy_index);
    const int row = index.row();
    const FileSearchResult *result = search_model_->resultAt(row);
    if (!result)
        return;

    QItemSelectionModel *selections = this->selectionModel();
    QModelIndexList selected = selections->selectedRows();
    if (selected.size() == 1) {
        search_item_.reset(new FileSearchResult(*result));
    } else {
        return;
    }

    context_menu_->exec(position); // synchronously
    search_item_.reset(NULL);
}

void SearchItemsTableView::onItemDoubleClick(const QModelIndex& index)
{
    const FileSearchResult *result =
            search_model_->resultAt(proxy_model_->mapToSource(index).row());
    if (result->name.isEmpty() || result->fullpath.isEmpty())
        return;

    if (result->fullpath.endsWith("/"))
        emit clearSearchBar();

    QString repo_name;
    if (gui->rpcClient()->getRepoUnameById(result->repo_id, &repo_name)) {
        QString path_to_open = ::pathJoin(gui->mountDir(), repo_name, ::getParentPath(result->fullpath));
        QDesktopServices::openUrl(QUrl::fromLocalFile(path_to_open));
    }
}

void SearchItemsTableView::setModel(QAbstractItemModel* model)
{
    search_model_ = qobject_cast<SearchItemsTableModel*>(model);
    if (!search_model_)
        return;
    proxy_model_ = new SearchSortFilterProxyModel(search_model_);
    proxy_model_->setSourceModel(search_model_);
    QTableView::setModel(proxy_model_);

    connect(model, SIGNAL(modelAboutToBeReset()), this, SLOT(onAboutToReset()));
    setSortingEnabled(true);

    // set default sort by folder
    sortByColumn(FILE_COLUMN_NAME, Qt::AscendingOrder);
}

void SearchItemsTableView::onAboutToReset()
{
    search_item_.reset(NULL);
}


SearchItemsTableModel::SearchItemsTableModel(QObject* parent)
    : QAbstractTableModel(parent),
      name_column_width_(kNameColumnWidth)
{

}

int SearchItemsTableModel::columnCount(const QModelIndex& parent) const
{
    return FILE_MAX_COLUMN;
}

int SearchItemsTableModel::rowCount(const QModelIndex& parent) const
{
    return results_.size();
}

void SearchItemsTableModel::setSearchResult(const std::vector<FileSearchResult> &results)
{
    beginResetModel();
    results_ = results;
    endResetModel();
}

QVariant SearchItemsTableModel::data(
    const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= (int)results_.size()) {
        return QVariant();
    }

    const int column = index.column();
    const int row = index.row();
    const FileSearchResult& result = results_[row];

    if (role == Qt::DecorationRole && column == FILE_COLUMN_NAME) {
        QIcon icon;

        if (result.fullpath.endsWith("/")) {
            icon = QIcon(":/images/files_v2/file_folder.png");
        } else {
            icon = QIcon(getIconByFileNameV2(result.name));
        }

        return icon.pixmap(kColumnIconSize, kColumnIconSize);
    }

    if (role == Qt::SizeHintRole) {
        QSize qsize(kDefaultColumnWidth, kDefaultColumnHeight);
        switch (column) {
        case FILE_COLUMN_NAME:
            qsize.setWidth(name_column_width_);
            break;
        case FILE_COLUMN_SIZE:
        case FILE_COLUMN_MTIME:
        case FILE_COLUMN_KIND:
        default:
            break;
        }
        return qsize;
    }

    if (role == Qt::UserRole && column == FILE_COLUMN_KIND) {
        return result.fullpath.endsWith("/") ? readableNameForFolder() : readableNameForFile(result.name);
    }

    //DisplayRole
    switch (column) {
    case FILE_COLUMN_NAME:

        return result.name;
    case FILE_COLUMN_SIZE:
        if (result.fullpath.endsWith("/"))
            return "";
        return result.size;
    case FILE_COLUMN_MTIME:
        return result.last_modified;
    case FILE_COLUMN_KIND:
    default:
        return QVariant();
    }
}

QVariant SearchItemsTableModel::headerData(int section,
                                             Qt::Orientation orientation,
                                             int role) const
{
    if (orientation == Qt::Vertical) {
        return QVariant();
    }

    if (role == Qt::TextAlignmentRole) {
        return Qt::AlignLeft + Qt::AlignVCenter;
    }

    if (role == Qt::DisplayRole) {
        switch (section) {
        case FILE_COLUMN_NAME:
            return tr("Name");
        case FILE_COLUMN_SIZE:
            return tr("Size");
        case FILE_COLUMN_MTIME:
            return tr("Last Modified");
        case FILE_COLUMN_KIND:
            return tr("Kind");
        default:
            return QVariant();
        }
    }

    if (role == Qt::FontRole) {
        QFont font;
        font.setPixelSize(12);
        return font;
    }

    if (role == Qt::ForegroundRole) {
        return QBrush(kFontColor);
    }

    if (role == Qt::SizeHintRole && section == FILE_COLUMN_NAME) {
        if (results_.empty()) {
            return QSize(name_column_width_, 0);
        }
    }

    return QVariant();
}

const FileSearchResult* SearchItemsTableModel::resultAt(int row) const
{
    if (row >= results_.size())
        return NULL;

    return &results_[row];
}

void SearchItemsTableModel::onResize(const QSize& size)
{
    name_column_width_ = size.width() - kDefaultColumnWidth * (FILE_MAX_COLUMN - 1);
    if (results_.empty())
        return;
    emit dataChanged(index(0, FILE_COLUMN_NAME),
                     index(rowCount()-1 , FILE_COLUMN_NAME));
}

bool SearchSortFilterProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    bool is_dir_left = search_model_->resultAt(left.row())->fullpath.endsWith("/");
    bool is_dir_right = search_model_->resultAt(right.row())->fullpath.endsWith("/");
    if (is_dir_left != is_dir_right) {
        return sortOrder() != Qt::AscendingOrder ? is_dir_right
                                                 : !is_dir_right;
    }
//    else if ((left.column() == FILE_COLUMN_NAME) &&
//             (right.column() == FILE_COLUMN_NAME)) {
//        const QString left_name = search_model_->resultAt(left.row())->name;
//        const QString right_name = search_model_->resultAt(right.row())->name;
//        return digitalCompare(left_name, right_name) < 0;
//    }
//    else {
//        return QSortFilterProxyModel::lessThan(left, right);
//    }
}

SearchItemsDelegate::SearchItemsDelegate(QObject *parent)
   : QStyledItemDelegate(parent)
{
}

void SearchItemsDelegate::paint(QPainter *painter,
                                 const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    const SearchItemsTableModel* model =
        static_cast<const SearchItemsTableModel*>(index.model());

    QRect option_rect = option.rect;

    painter->save();
    if (option.state & QStyle::State_Selected) {
        painter->fillRect(option_rect, kSelectedItemBackgroundcColor);
    }
    else
        painter->fillRect(option_rect, kItemBackgroundColor);
    painter->restore();

    //
    // draw item's border
    //

    // draw item's border for the first row only
    static const QPen borderPen(kItemBottomBorderColor, 1);
    if (index.row() == 0) {
        painter->save();
        painter->setPen(borderPen);
        painter->drawLine(option_rect.topLeft(), option_rect.topRight());
        painter->restore();
    }
    // draw item's border under the bottom
    painter->save();
    painter->setPen(borderPen);
    painter->drawLine(option_rect.bottomLeft(), option_rect.bottomRight());
    painter->restore();

    QSize size = model->data(index, Qt::SizeHintRole).value<QSize>();
    QString text = model->data(index, Qt::DisplayRole).value<QString>();

    switch (index.column()) {
    case FILE_COLUMN_NAME:
    {
        // draw icon
        QPixmap pixmap = model->data(index, Qt::DecorationRole).value<QPixmap>();
        double scale_factor = globalDevicePixelRatio();
        // On Mac OSX (and other HDPI screens) the pixmap would be the 2x
        // version (but the draw rect area is still the same size), so when
        // computing the offsets we need to divide it by the scale factor.
        int icon_width = qMin(kColumnIconSize,
                             int((double)pixmap.width() / (double)scale_factor));
        int icon_height = qMin(size.height(),
                               int((double)pixmap.height() / (double)scale_factor));
        int alignX = (kColumnIconSize - icon_width) / 2;
        int alignY = (size.height() - icon_height) / 2;

#ifdef Q_OS_WIN32
    kMarginLeft = 4;
#endif

        QRect icon_bound_rect(
            option_rect.topLeft() + QPoint(kMarginLeft + alignX, alignY - 2),
            QSize(icon_width, icon_height));

        painter->save();
        painter->drawPixmap(icon_bound_rect, pixmap);
        painter->restore();

        // draw text
        QFont font = model->data(index, Qt::FontRole).value<QFont>();
        QRect rect(option_rect.topLeft() + QPoint(kMarginLeft + 2 * 2 + kColumnIconSize, -2),
                   size - QSize(kColumnIconSize + kMarginLeft, 0));
        painter->setPen(kFileNameFontColor);
        painter->setFont(font);
        painter->drawText(
            rect,
            Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
            fitTextToWidth(
                text,
                option.font,
                rect.width() - kMarginBetweenFileNameAndStatusIcon - kFileStatusIconSize - 5));
    }
        break;
    case FILE_COLUMN_SIZE:
        if (!text.isEmpty())
            text = ::readableFileSize(model->data(index, Qt::DisplayRole).value<quint64>());
    case FILE_COLUMN_MTIME:
        if (index.column() == FILE_COLUMN_MTIME)
            text = ::translateCommitTime(model->data(index, Qt::DisplayRole).value<quint64>());
    case FILE_COLUMN_KIND:
    {
        if (index.column() == FILE_COLUMN_KIND) {
            text = model->data(index, Qt::UserRole).toString();
        }
        QFont font = model->data(index, Qt::FontRole).value<QFont>();
        QRect rect(option_rect.topLeft() + QPoint(9, -2), size - QSize(10, 0));
        painter->save();
        painter->setPen(kFontColor);
        painter->setFont(font);
        painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text);
        painter->restore();
    }
         break;
    default:
        qWarning() << "invalid item (row)";
        break;
    }
}
