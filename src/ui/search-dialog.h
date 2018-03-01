#ifndef SEAFILE_CLIENT_SEARCH_DIALOG_H
#define SEAFILE_CLIENT_SEARCH_DIALOG_H
#include <QDialog>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QSortFilterProxyModel>
#include <QTimer>
#include <vector>

#include "search-bar.h"
#include "api/requests.h"
#include "account.h"

class ApiError;
class QToolBar;
class QToolButton;
class QStackedWidget;
class QLabel;
class SearchBar;
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
    void onRefresh();
    void doSearch(const QString& keyword);
    void doRealSearch();
    void onSearchSuccess(const std::vector<FileSearchResult>& results,
                         bool is_loading_more,
                         bool has_more);
    void onSearchFailed(const ApiError& error);

private:
    void closeEvent(QCloseEvent *ev);
    void createToolBar();
    void createLoadingFailedView();
    void createEmptyView();
    void createTable();

    bool eventFilter(QObject *obj, QEvent *event);

    const Account account_;

    QTimer *search_timer_;
    FileSearchRequest *search_request_;
    qint64 search_text_last_modified_;

    QToolBar *toolbar_;
//    QToolButton *refresh_button_;
    QStackedWidget *stack_;
    QLabel *loading_failed_view_;
    QWidget *waiting_view_;

    SearchBar *search_bar_;
    SearchItemsTableView* search_view_;
    SearchItemsTableModel* search_model_;
    SearchItemsDelegate* search_delegate_;
};


class SearchDialog;
class SearchItemsTableModel;
struct FileSearchResult;
class FileSearchRequest;
class SearchItemsTableView : public QTableView
{
    Q_OBJECT
public:
    SearchItemsTableView(QWidget* parent = 0);
    void resizeEvent(QResizeEvent* event) Q_DECL_OVERRIDE;
    void setModel(QAbstractItemModel* model) Q_DECL_OVERRIDE;
    void setupContextMenu();
signals:
    void clearSearchBar();
private slots:
    void onAboutToReset();
    void onItemDoubleClick(const QModelIndex& index);

private:

    SearchDialog *parent_;
    SearchItemsTableModel* search_model_;

    QScopedPointer<const FileSearchResult> search_item_;

    QMenu *context_menu_;
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
    QVariant data(const QModelIndex& index,
                  int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role) const Q_DECL_OVERRIDE;
    void setSearchResult(const std::vector<FileSearchResult>& results);

    void onResize(const QSize &size);
    const FileSearchResult* resultAt(int row) const;

private:
    std::vector<FileSearchResult> results_;

    int name_column_width_;
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
