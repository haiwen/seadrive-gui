#ifndef SEAFILE_CLIENT_TRANSFER_PROGRESS_DIALOG_H
#define SEAFILE_CLIENT_TRANSFER_PROGRESS_DIALOG_H
#include <QDialog>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QHeaderView>
#include <QTimer>
#include "utils/json-utils.h"

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
// only available in qt 5.0+
#define Q_DECL_OVERRIDE
#endif

enum TransferType {
    UPLOAD,
    DOWNLOAD,
};

struct TransferringInfo {
    QString file_path;
    quint64 last_second_bytes;
    quint64 transferred_bytes;
    quint64 total_bytes;
};

struct TransferredInfo {
    QString file_path;
    quint64 total_bytes = 0;
};

class TransferItemsTableView;
class TransferItemsTableModel;

class TransferProgressDialog : public QDialog
{
    Q_OBJECT
public:
    TransferProgressDialog(QWidget *parent = 0);

private slots:

private:
    void createTable();

    TransferItemsTableView* table_;
    TransferItemsTableModel* model_;
};


class TransferItemsHeadView : public QHeaderView
{
    Q_OBJECT
public:
    TransferItemsHeadView(QWidget* parent = 0);
    QSize sectionSizeFromContents(int index) const Q_DECL_OVERRIDE;
};


class TransferItemsTableView : public QTableView
{
    Q_OBJECT
public:
    TransferItemsTableView(QWidget* parent = 0);
    void resizeEvent(QResizeEvent* event) Q_DECL_OVERRIDE;
    void setModel(QAbstractItemModel* model) Q_DECL_OVERRIDE;
    TransferItemsTableModel* sourceModel();

private:
    TransferItemsTableModel* source_model_;
};


class TransferItemsTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    TransferItemsTableModel(QObject* parent = 0);
    void setTransferItems();

    int rowCount(const QModelIndex& parent = QModelIndex()) const
        Q_DECL_OVERRIDE;
    int columnCount(const QModelIndex& parent = QModelIndex()) const
        Q_DECL_OVERRIDE;
    QVariant data(const QModelIndex& index,
                  int role = Qt::DisplayRole) const Q_DECL_OVERRIDE;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role) const Q_DECL_OVERRIDE;
    const TransferringInfo* itemAt(int row) const;
    uint nameColumnWidth() const;

signals:

public slots:
    void onResize(const QSize& size);
    void setTransferType(const int);

private slots:
    void updateTransferringInfo();

private:
    uint name_column_width_;
    QTimer *progress_timer_;
    TransferType transfer_type_;

    QList<TransferringInfo> uploading_files_, downloading_files_;
    QList<TransferredInfo> uploaded_files_, downloaded_files_;
};


class TransferItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    TransferItemDelegate(QObject *parent);
    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const;
};

#endif
