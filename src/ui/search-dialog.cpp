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
#include "ui/loading-view.h"
#include "file-provider-mgr.h"

namespace
{
enum {
    FILE_COLUMN_NAME = 0,
    FILE_COLUMN_KIND,
    FILE_MAX_COLUMN
};

enum {
    INDEX_WAITING_VIEW,
    INDEX_LOADING_VIEW,
    INDEX_LOADING_FAILED_VIEW,
    INDEX_EMPTY_VIEW,
    INDEX_SEARCH_VIEW
};


const int kDefaultColumnWidth = 120;
const int kDefaultColumnHeight = 40;
const char *kLoadingFailedLabelName = "LoadingFailedText";
const int kToolBarIconSize = 24;
const int kInputDelayInterval = 300;
//const int kStatusBarIconSize = 20;

const int kColumnIconSize = 28;
const int kFileNameColumnWidth = 200;
const int kFileStatusIconSize = 16;
const int kMarginBetweenFileNameAndStatusIcon = 5;

#ifdef Q_OS_WIN32
const int kMarginLeft = 4;
#else
const int kMarginLeft = 5;
#endif

const int kFileNameHeight = 12;
const int kSubtitleHeight = 5;
const int kExtraPadding = 30;

const QColor kFileNameFontColor("black");
const QColor kFontColor("#757575");
const QColor kSelectedItemBackgroundcColor("#F9E0C7");
const QColor kItemBackgroundColor("white");
const QColor kItemBottomBorderColor("#f3f3f3");
const QColor kSubtitleColor("#959595");

const int SubtitleRole = Qt::UserRole + 1;

const int PLACE_HOLDER_TYPE = 999;

const int ResultAtRole = Qt::UserRole + 1;

static inline const QTableWidgetItem *getItem(const QModelIndex &index)
{
    const SearchItemsTableModel *model = static_cast<const SearchItemsTableModel*>(index.model());
    return model->item(index);
}
static inline FileSearchResult getSearchResult(const QModelIndex &index)
{
    const QTableWidgetItem *item = getItem(index);
    if (!item)
        return FileSearchResult();
    return item->data(ResultAtRole).value<FileSearchResult>();
}

} // namespace

SearchDialog::SearchDialog(QWidget *parent)
    : QDialog(parent),
      search_request_(NULL),
      search_text_last_modified_(0),
      nth_page_(1)
{
    loading_row_ = 0;
    setWindowTitle(tr("Search files"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    // Note: do not add the Qt::WindowStaysOnTopHint flag because it
    // would make the menu shown below the dialog when right-click for
    // the second time.
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint));

    setMinimumSize(QSize(600, 371));
    createToolBar();
    createFilterMenu();
    createLoadingView();
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

//    QHBoxLayout *hlayout = new QHBoxLayout;
//    hlayout->setContentsMargins(1, 0, 1, 0);
//    hlayout->setSpacing(0);
//    hlayout->addWidget(toolbar_);

    stack_ = new QStackedWidget;
    stack_->insertWidget(INDEX_WAITING_VIEW, waiting_view_);
    stack_->insertWidget(INDEX_LOADING_VIEW, loading_view_);
    stack_->insertWidget(INDEX_LOADING_FAILED_VIEW, loading_failed_view_);
    stack_->insertWidget(INDEX_EMPTY_VIEW, empty_view_);
    stack_->insertWidget(INDEX_SEARCH_VIEW, search_view_);
    stack_->setContentsMargins(0, 0, 0, 0);
    stack_->installEventFilter(this);
    stack_->setAcceptDrops(true);

    //vlayout->addLayout(hlayout);
    vlayout->addWidget(toolbar_);
    vlayout->addWidget(filter_menu_);
    vlayout->addWidget(stack_);

    search_timer_ = new QTimer(this);
    connect(search_timer_, SIGNAL(timeout()), this, SLOT(doRealSearch()));
    search_timer_->start(kInputDelayInterval);

//    connect(search_view_, SIGNAL(clearSearchBar()),
//            search_bar_, SLOT(clear()));
}

SearchDialog::~SearchDialog()
{
    if (search_request_ != NULL)
        search_request_->deleteLater();
}

void SearchDialog::closeEvent(QCloseEvent *ev)
{
    emit aboutClose();
    ev->accept();
}

void SearchDialog::createToolBar()
{
    toolbar_ = new QToolBar;
    toolbar_->setObjectName("topBar");
    toolbar_->setIconSize(QSize(kToolBarIconSize, kToolBarIconSize));
    toolbar_->setStyleSheet("QToolbar { spacing: 0px; }");

    search_account_ = new QComboBox;
    auto accounts = gui->accountManager()->activeAccounts();
    for (int i = 0; i < accounts.size(); i++) {
        auto account = accounts.at(i);
        if (account.isPro()) {
            QString item_name = QString("%1 (%2)").arg(account.accountInfo.name).arg(account.serverUrl.toString());
            search_account_->insertItem(i, item_name, QVariant::fromValue(account));
        }
    }
    toolbar_->addWidget(search_account_);
    connect(search_account_, SIGNAL(currentIndexChanged(int)),
            this, SLOT(changeAccount(int)));

    search_bar_ = new SearchBar;
    search_bar_->setPlaceholderText(tr("Search files"));
    toolbar_->addWidget(search_bar_);
    connect(search_bar_, SIGNAL(textEdited(const QString&)),
            this, SLOT(doSearch()));
    connect(search_bar_, SIGNAL(editingFinished()),
            this, SLOT(doSearch()));

    search_all_file_ = new QRadioButton;
    search_all_file_->setText(tr("All file types"));
    search_all_file_->setChecked(true);
    connect(search_all_file_, SIGNAL(toggled(bool)), this, SLOT(closeFilterMenu(bool)));

    search_custom_file_ = new QRadioButton;
    search_custom_file_->setText(tr("Custom file type"));
    connect(search_custom_file_, SIGNAL(toggled(bool)), this, SLOT(openFilterMenu()));
    toolbar_->addWidget(search_all_file_);
    toolbar_->addWidget(search_custom_file_);
//    refresh_button_ = new QToolButton;
//    refresh_button_->setObjectName("refreshButton");
//    refresh_button_->setToolTip(tr("Refresh"));
//    refresh_button_->setIcon(QIcon(":/images/filebrowser/refresh-gray.png"));
//    refresh_button_->setIconSize(QSize(kStatusBarIconSize, kStatusBarIconSize));
//    refresh_button_->installEventFilter(this);
//    connect(refresh_button_, SIGNAL(clicked()), this, SLOT(onRefresh()));
//    toolbar_->addWidget(refresh_button_);
}

void SearchDialog::createFilterMenu()
{
    filter_menu_ = new FilterMenu;
    filter_menu_->setVisible(false);
    connect(filter_menu_, SIGNAL(filterChanged()),
            this, SLOT(onRefresh()));
}

void SearchDialog::createLoadingView()
{
    loading_view_ = new LoadingView;
    static_cast<LoadingView*>(loading_view_)->setQssStyleForTab();

    loading_failed_view_ = new QLabel;
    loading_failed_view_->setObjectName(kLoadingFailedLabelName);
    QString link = QString("<a style=\"color:#777\" href=\"#\">%1</a>").arg(tr("retry"));
    QString label_text = tr("Failed to search<br/>"
                            "Please %1").arg(link);
    loading_failed_view_->setText(label_text);
    loading_failed_view_->setAlignment(Qt::AlignCenter);

    connect(loading_failed_view_, SIGNAL(linkActivated(const QString&)),
            this, SLOT(onRefresh()));
}

void SearchDialog::onRefresh(bool loading_more)
{
    QStringList filter_list = filter_menu_->filterList();
    if (!search_bar_->text().isEmpty()) {
        search_text_last_modified_ = 1;
        if (!filter_list.isEmpty()) {
            doRealSearch(loading_more, false, filter_list);
        } else {
            doRealSearch(loading_more);
        }
    }
}

void SearchDialog::createEmptyView()
{
    waiting_view_ = new QWidget;
    waiting_view_->installEventFilter(this);

    empty_view_ = new QLabel(this);
    empty_view_->setText(tr("No result found"));
    empty_view_->setAlignment(Qt::AlignCenter);
    empty_view_->setStyleSheet("background-color: white");
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
    if (obj == waiting_view_ && event->type() == QEvent::Paint) {
        QPainter painter(waiting_view_);

        QPaintEvent *ev = (QPaintEvent*)event;
        const QSize size(72, 72);
        const int x = ev->rect().width() / 2 - size.width() / 2;
        const int y = ev->rect().height() / 2 - size.height() / 2;
        QRect rect(QPoint(x, y), size);

        QPixmap image = QIcon(":/images/main-panel/search-background.png").pixmap(size);
        painter.drawPixmap(rect, image);

        return true;
    };
    return QObject::eventFilter(obj, event);
}

void SearchDialog::openFilterMenu()
{
    filter_menu_->setVisible(true);
}

void SearchDialog::closeFilterMenu(bool checked)
{
    if (checked) {
        filter_menu_->setVisible(false);
        filter_menu_->clearCheckBox();
        onRefresh();
    }
}

void SearchDialog::changeAccount(int index)
{
    stack_->setCurrentIndex(INDEX_WAITING_VIEW);

    auto account = search_account_->currentData().value<Account>();
    search_model_->account_ = account;
}

void SearchDialog::doSearch()
{
    // make it search utf-8 charcters
    if (search_bar_->text().toUtf8().size() <= 3) {
        if (search_request_) {
            search_request_->deleteLater();
            search_request_ = nullptr;
        }

        stack_->setCurrentIndex(INDEX_WAITING_VIEW);
        search_text_last_modified_ = 0;
        return;
    }

    // save for doRealSearch
    search_text_last_modified_ = QDateTime::currentMSecsSinceEpoch();
}

void SearchDialog::doRealSearch(bool load_more,
                                bool isAll,
                                const QStringList& filter_list)
{
    if (!load_more) {
        // not modified
        if (search_text_last_modified_ == 0)
            return;
        // modified too fast
        if (QDateTime::currentMSecsSinceEpoch() - search_text_last_modified_ <= kInputDelayInterval)
            return;
        //search again, the ‘loading more’ is reindexed
        loading_row_ = 0;
    }

    if (!search_model_->account_.isValid())
        return;

    if (search_bar_->text().isEmpty()) {
        return;
    }

    if (search_request_) {
        // search_request_->abort();
        search_request_->deleteLater();
        search_request_ = NULL;
    }

    if (!load_more) {
        nth_page_ = 1;
        stack_->setCurrentIndex(INDEX_LOADING_VIEW);
    } else {
        nth_page_++;
    }

    QString allOrCustom;
    if (isAll) {
        allOrCustom = QString("all");
    } else {
        allOrCustom = QString("custom");
    }

    search_request_ = new FileSearchRequest(search_model_->account_, search_bar_->text(), filter_list, allOrCustom, nth_page_);
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
    // The response should be from the latest request.
    if (sender() != search_request_) {
        return;
    }

    if (results.size() == 0) {
        stack_->setCurrentIndex(INDEX_EMPTY_VIEW);
    } else {
        stack_->setCurrentIndex(INDEX_SEARCH_VIEW);
    }

    std::vector<QTableWidgetItem*> items;

    for (unsigned i = 0; i < results.size(); ++i) {
        QTableWidgetItem *item = new QTableWidgetItem(results[i].name);
        item->setData(ResultAtRole, QVariant::fromValue(results[i]));
        items.push_back(item);
    }

    const QModelIndex first_new_item = search_model_->updateSearchResults(items, is_loading_more, has_more);
    if (first_new_item.isValid()) {
        search_view_->scrollTo(first_new_item);
    }

    int old_loading_row = loading_row_;
    search_view_->setRowHeight(old_loading_row, 40);
    search_view_->clearSpans();
    if (has_more) {
        loading_row_ += items.size();
        load_more_btn_ = new LoadMoreButton;
        connect(load_more_btn_, SIGNAL(clicked()),
                this, SLOT(loadMoreSearchResults()));

        search_view_->setRowHeight(loading_row_, 80);
        search_view_->setSpan(loading_row_, 0, loading_row_, 4);
        search_view_->setIndexWidget(
            search_model_->loadMoreIndex(), load_more_btn_);
    }
}

void SearchDialog::onSearchFailed(const ApiError& error)
{
    // The response should be from the latest request.
    if (sender() != search_request_) {
        return;
    }

    stack_->setCurrentIndex(INDEX_LOADING_FAILED_VIEW);
}

void SearchDialog::loadMoreSearchResults()
{
    onRefresh(true);
}

SearchItemsTableView::SearchItemsTableView(QWidget* parent)
    : QTableView(parent),
      parent_(qobject_cast<SearchDialog*>(parent)),
      search_model_(NULL)
{
    verticalHeader()->hide();
    verticalHeader()->setDefaultSectionSize(kDefaultColumnHeight);
    horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setCascadingSectionResizes(true);
    horizontalHeader()->setHighlightSections(false);
//    horizontalHeader()->setSortIndicatorShown(true);
    horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    setGridStyle(Qt::NoPen);
    setShowGrid(false);
    setContentsMargins(0, 5, 0, 5);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);

    setMouseTracking(true);

    connect(this, SIGNAL(doubleClicked(const QModelIndex&)),
                this, SLOT(onItemDoubleClick(const QModelIndex&)));
    setupContextMenu();
}

void SearchItemsTableView::setupContextMenu()
{
    context_menu_ = new QMenu(this);
    connect(parent_, SIGNAL(aboutClose()),
            context_menu_, SLOT(close()));
    open_file_action_ = new QAction(tr("&Open the file"), this);
    connect(open_file_action_, SIGNAL(triggered()),
            this, SLOT(openFile()));
    open_directory_action_ = new QAction(tr("&Show in folder"), this);
    connect(open_directory_action_, SIGNAL(triggered()),
            this, SLOT(openDirectory()));

    context_menu_->addAction(open_file_action_);
    context_menu_->addAction(open_directory_action_);

    this->addAction(open_file_action_);
    this->addAction(open_directory_action_);
}

void SearchItemsTableView::contextMenuEvent(QContextMenuEvent *event)
{
    QPoint pos = event->pos();
    QModelIndex index = indexAt(pos);
    pos = viewport()->mapToGlobal(pos);
    if (!index.isValid()) {
        return;
    }

    const QTableWidgetItem* item = getItem(index);
    if (!item) {
        return;
    }

    QModelIndexList indexes = selectionModel()->selection().indexes();
    if (indexes.size() != 0) {
        FileSearchResult file = getSearchResult(indexes.at(0));
        open_directory_action_->setData(QVariant::fromValue(file));
        open_directory_action_->setEnabled(true);
        open_file_action_->setEnabled(true);
    } else {
        open_directory_action_->setEnabled(false);
        open_file_action_->setEnabled(false);
    }
    context_menu_->exec(pos);
}

void SearchItemsTableView::openFile()
{
    openDirectory(true);
}

void SearchItemsTableView::openDirectory()
{
    openDirectory(false);
}

void SearchItemsTableView::openDirectory(bool open_file)
{
    FileSearchResult result =
        qvariant_cast<FileSearchResult>(open_directory_action_->data());
    QString repo_name;
    auto client = gui->rpcClient(search_model_->account_.domainID());
    if (!client->getRepoUnameById(result.repo_id, &repo_name)) {
        gui->warningBox(tr("File not found, maybe not synchorized yet"), this);
        return;
    }

#ifdef Q_OS_MAC
    QString root = gui->fileProviderManager()->rootPath(search_model_->account_);
#endif
#ifdef Q_OS_WIN32
    QString root = search_model_->account_.syncRoot;
#endif
#ifdef Q_OS_LINUX
    QString root = gui->seadriveRoot();
    if (gui->accountManager()->activeAccounts().size() > 1) {
        root = ::pathJoin(root, search_model_->account_.displayName);
    }
#endif
    if (root.isEmpty()) {
        qWarning() << "failed to get root path";
        return;
    }

    QString path = ::pathJoin(root, repo_name, result.fullpath);
    if (open_file) {
        openUrl(QUrl::fromLocalFile(path));
    } else {
        showInGraphicalShell(path);
    }
}

void SearchItemsTableView::resizeEvent(QResizeEvent* event)
{
    QTableView::resizeEvent(event);
    if (search_model_)
        search_model_->onResize(event->size());
}

void SearchItemsTableView::onItemDoubleClick(const QModelIndex& index)
{
    FileSearchResult result = getSearchResult(index);
    if (result.name.isEmpty() || result.fullpath.isEmpty())
        return;

//    if (result.fullpath.endsWith("/"))
//        emit clearSearchBar();

    QString repo_name;
    auto client = gui->rpcClient(search_model_->account_.domainID());
    if (!client->getRepoUnameById(result.repo_id, &repo_name)) {
        gui->warningBox(tr("File not found, maybe not synchorized yet"), this);
        return;
    }

#ifdef Q_OS_MAC
    QString root = gui->fileProviderManager()->rootPath(search_model_->account_);
#endif
#ifdef Q_OS_WIN32
    QString root = search_model_->account_.syncRoot;
#endif
#ifdef Q_OS_LINUX
    QString root = gui->seadriveRoot();
    if (gui->accountManager()->activeAccounts().size() > 1) {
        root = ::pathJoin(root, search_model_->account_.displayName);
    }
#endif
    if (root.isEmpty()) {
        qWarning() << "failed to get root path";
        return;
    }

    QString path = ::pathJoin(root, repo_name, result.fullpath);
    showInGraphicalShell(path);
}

void SearchItemsTableView::setModel(QAbstractItemModel* model)
{
    search_model_ = qobject_cast<SearchItemsTableModel*>(model);
    if (!search_model_)
        return;
    QTableView::setModel(search_model_);

    connect(model, SIGNAL(modelAboutToBeReset()), this, SLOT(onAboutToReset()));

    // set default sort by folder
    //sortByColumn(FILE_COLUMN_NAME, Qt::AscendingOrder);
}

void SearchItemsTableView::onAboutToReset()
{
    search_item_.reset(NULL);
}


SearchItemsTableModel::SearchItemsTableModel(QObject* parent)
    : QAbstractTableModel(parent),
      name_column_width_(kFileNameColumnWidth)
{
    auto accounts = gui->accountManager()->activeAccounts();
    account_ = accounts.first();
}

int SearchItemsTableModel::columnCount(const QModelIndex& parent) const
{
    return FILE_MAX_COLUMN;
}

int SearchItemsTableModel::rowCount(const QModelIndex& parent) const
{
    return items_.size();
}

const QModelIndex SearchItemsTableModel::updateSearchResults(
    const std::vector<QTableWidgetItem *> &items,
    bool is_loading_more,
    bool has_more)
{
    int first_new_item = 0;

    beginResetModel();
    if (!is_loading_more) {
        first_new_item = 0;
        clear();
    } else {
        if (items_.size() > 0 && items_[items_.size() - 1]->type() == PLACE_HOLDER_TYPE) {
            QTableWidgetItem *old_place_holder = items_[items_.size() - 1];
            items_.pop_back();
            first_new_item = items_.size();

            delete old_place_holder;
        }
    }

    items_.insert(items_.end(), items.begin(), items.end());

    // place holder for the "load more" button
    QTableWidgetItem *load_more_place_holder = new QTableWidgetItem(nullptr, PLACE_HOLDER_TYPE);
    items_.push_back(load_more_place_holder);

    load_more_index_ = QModelIndex();
    if (has_more) {
        load_more_index_ = index(items_.size() - 1, 3);
    }

    endResetModel();

    if (first_new_item) {
        return index(first_new_item, 0);
    }
    return QModelIndex();
}

QVariant SearchItemsTableModel::data(
    const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= (int)items_.size()) {
        return QVariant();
    }

    const int column = index.column();
    FileSearchResult result = getSearchResult(index);

    if (role == Qt::DecorationRole && column == FILE_COLUMN_NAME) {
        QIcon icon;

        if (result.fullpath.endsWith("/")) {
            icon = QIcon(":/images/files_v2/file_folder.png");
        } else {
            icon = QIcon(getIconByFileNameV2(result.name));
        }

        return icon.pixmap(kColumnIconSize, kColumnIconSize);
    }

    if (role == SubtitleRole && column == FILE_COLUMN_NAME) {
        int count_of_splash = result.fullpath.endsWith("/") ? 2 : 1;
        QString subtitle = result.fullpath.mid(1, result.fullpath.size() - count_of_splash - result.name.size());
        if (!subtitle.isEmpty())
            subtitle = result.repo_name + "/" + subtitle.left(subtitle.size() - 1);
        else
            subtitle = result.repo_name;
        return subtitle;
    }

    if (role == Qt::SizeHintRole) {
        QSize qsize(kDefaultColumnWidth, kDefaultColumnHeight);
        switch (column) {
        case FILE_COLUMN_NAME:
            qsize.setWidth(name_column_width_);
            break;
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
#if(QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        return static_cast<Qt::Alignment::Int>(Qt::AlignLeft | Qt::AlignVCenter);
#else
        return Qt::AlignLeft + Qt::AlignVCenter;
#endif
    }

    if (role == Qt::DisplayRole) {
        switch (section) {
        case FILE_COLUMN_NAME:
            return tr("Name");
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
        if (items_.empty()) {
            return QSize(name_column_width_, 0);
        }
    }

    return QVariant();
}

void SearchItemsTableModel::onResize(const QSize& size)
{
    name_column_width_ = size.width() - kDefaultColumnWidth * (FILE_MAX_COLUMN - 1) - kExtraPadding;
    if (items_.empty())
        return;
    emit dataChanged(index(0, FILE_COLUMN_NAME),
                     index(rowCount()-1 , FILE_COLUMN_NAME));
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

    const QTableWidgetItem* item = model->item(index);
    if (item && item->type() == PLACE_HOLDER_TYPE) {
        // This is the place holder item for the "load more" button
        return;
    }

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

        QRect icon_bound_rect(
            option_rect.topLeft() + QPoint(kMarginLeft + alignX, alignY - 2),
            QSize(icon_width, icon_height));

        painter->save();
        painter->drawPixmap(icon_bound_rect, pixmap);
        painter->restore();

        // draw text
        QRect rect(option_rect.topLeft() + QPoint(kMarginLeft + 2 * 2 + kColumnIconSize, -2),
                   size - QSize(kColumnIconSize + kMarginLeft, kFileNameHeight));
        painter->setPen(kFileNameFontColor);
        painter->drawText(
            rect,
            Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
            fitTextToWidth(
                text,
                option.font,
                rect.width() - kMarginBetweenFileNameAndStatusIcon - kFileStatusIconSize - 5));

        //
        // Paint repo_name
        //
        QString subtitle = model->data(index, SubtitleRole).value<QString>();
        QFont subtitle_font;
        subtitle_font.setPixelSize(10);
        QRect subtitle_rect(option_rect.topLeft() + QPoint(kMarginLeft + 2 * 2 + kColumnIconSize, -2),
                   size - QSize(kColumnIconSize + kMarginLeft, kSubtitleHeight));
        painter->save();
        painter->setPen(kSubtitleColor);
        painter->setFont(subtitle_font);
        painter->drawText(subtitle_rect,
                          Qt::AlignLeft | Qt::AlignBottom,
                          fitTextToWidth(subtitle, option.font, subtitle_rect.width() - kMarginBetweenFileNameAndStatusIcon - kFileStatusIconSize - 5),
                          &subtitle_rect);
        painter->restore();
    }
        break;
    case FILE_COLUMN_KIND:
    {
        if (index.column() == FILE_COLUMN_KIND) {
            text = model->data(index, Qt::UserRole).toString();
        }
        QRect rect(option_rect.topLeft() + QPoint(9, -2), size - QSize(10, 0));
        painter->save();
        painter->setPen(kFontColor);
        painter->drawText(rect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text);
        painter->restore();
    }
         break;
    default:
        qWarning() << "invalid item (row)";
        break;
    }
}
