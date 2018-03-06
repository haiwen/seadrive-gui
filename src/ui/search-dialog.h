#ifndef SEAFILE_CLIENT_SEARCH_DIALOG_H
#define SEAFILE_CLIENT_SEARCH_DIALOG_H
#include <QDialog>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTableWidgetItem>
#include <QTimer>
#include <vector>

#include "search-bar.h"
#include "api/requests.h"
#include "account.h"
#include "ui/filter-menu.h"

class ApiError;
class QToolBar;
class QStackedWidget;
class QLabel;
class LoadMoreButton;
class QRadioButton;
class QCheckBox;
class SearchBar;
class FilterMenu;
class SearchItemsTableView;
class SearchItemsTableModel;
class SearchItemsDelegate;
struct FileSearchResult;
class FileSearchRequest;

class SearchDialog : public QDialog
{
    Q_OBJECT
public:
    SearchDialog(const Account &account, QWidget *parent = 0);
    ~SearchDialog();
signals:
    void aboutClose();
private slots:
    void openFilterMenu();
    void closeFilterMenu();
    void onRefresh();
    void doSearch(const QString& keyword);
    void doRealSearch(bool load_more = false,
                      bool isAll = true,
                      const QStringList& filter_list = QStringList(),
                      const QString& input_fexts = QString());
    void onSearchSuccess(const std::vector<FileSearchResult>& results,
                         bool is_loading_more,
                         bool has_more);
    void onSearchFailed(const ApiError& error);
    void loadMoreSearchResults();

private:
    void closeEvent(QCloseEvent *ev);
    void createToolBar();
    void createFilterMenu();
    void createLoadingFailedView();
    void createEmptyView();
    void createTable();

    bool eventFilter(QObject *obj, QEvent *event);

    const Account account_;

    QTimer *search_timer_;
    FileSearchRequest *search_request_;
    qint64 search_text_last_modified_;

    //toolbar
    QToolBar *toolbar_;
//    QToolButton *refresh_button_;
    QRadioButton *search_all_file_;
    QRadioButton *search_custom_file_;

    //menu
    FilterMenu *filter_menu_;

    //stack
    QStackedWidget *stack_;
    QLabel *loading_failed_view_;
    QWidget *waiting_view_;

    LoadMoreButton *load_more_btn_;
    SearchBar *search_bar_;
    SearchItemsTableView* search_view_;
    SearchItemsTableModel* search_model_;
    SearchItemsDelegate* search_delegate_;

    int nth_page_;
    int loading_row_;
};

class SearchItemsTableView : public QTableView
{
    Q_OBJECT
public:
    SearchItemsTableView(QWidget* parent = 0);
    void resizeEvent(QResizeEvent* event) Q_DECL_OVERRIDE;
    void setModel(QAbstractItemModel* model) Q_DECL_OVERRIDE;
//signals:
//    void clearSearchBar();
private slots:
    void onAboutToReset();
    void onItemDoubleClick(const QModelIndex& index);

private:

    QScopedPointer<const FileSearchResult> search_item_;
    SearchDialog *parent_;
    // source model
    SearchItemsTableModel *search_model_;
};


class SearchItemsTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    SearchItemsTableModel(QObject* parent = 0);

    int rowCount(const QModelIndex& parent = QModelIndex()) const
        Q_DECL_OVERRIDE;
    int columnCount(const QModelIndex& parent = QModelIndex()) const
        Q_DECL_OVERRIDE;

    const QTableWidgetItem* item(const QModelIndex& index) const
    {
        if (!index.isValid() || index.row() >= (int)items_.size())
            return NULL;
        return items_[index.row()];
    }

    QVariant data(const QModelIndex& index,
                  int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role) const Q_DECL_OVERRIDE;
    const QModelIndex updateSearchResults(const std::vector<QTableWidgetItem *> &items, bool is_loading_more, bool has_more);
    const QModelIndex loadMoreIndex() const { return load_more_index_; }

    void onResize(const QSize &size);
    const FileSearchResult* resultAt(int row) const;

    void clear()
    {
        for (unsigned i = 0 ; i < items_.size(); ++i) {
            delete items_[i];
        }
        items_.clear();
    }

private:
    std::vector<QTableWidgetItem*> items_;

    int name_column_width_;
    QModelIndex load_more_index_;
};

class DataManager;
class ThumbnailService;
class SearchItemsDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    SearchItemsDelegate(QObject *parent);
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const;
};
#endif //SEAFILE_CLIENT_SEARCH_DIALOG_H
