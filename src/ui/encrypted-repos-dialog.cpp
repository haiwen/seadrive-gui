
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QLabel>
#include <QMenu>
#include <QAction>
#include <QInputDialog>

#include "encrypted-repos-dialog.h"

#include "utils/json-utils.h"
#include "utils/utils.h"
#include "rpc/rpc-client.h"
#include "seadrive-gui.h"


namespace {

enum {
    COLUMN_REPO_NAME = 0,
    COLUMN_IS_SET_PASSWORD,
    MAX_COLUMN,
};

enum {
    INDEX_EMPTY_VIEW = 0,
    INDEX_TABLE_VIEW
};

const int kDefaultColumnWidth = 120;
const int kDefaultColumnHeight = 40;

const int kRepoNameColumnWidth = 100;
const int kRepoStatus = 30;

const int kDefaultColumnSum = kRepoNameColumnWidth + kRepoStatus;

} //namespace


EncryptedRepoInfo EncryptedRepoInfo::fromJSON(const json_t *root) {

    EncryptedRepoInfo enc_repo_info;
    Json json(root);

    enc_repo_info.repo_id = json.getString("repo_id");
    enc_repo_info.repo_name = json.getString("repo_display_name");
    enc_repo_info.is_password_set  = json.getBool("is_passwd_set");
    return enc_repo_info;
}

QList<EncryptedRepoInfo> EncryptedRepoInfo::listFromJSON(const json_t *json) {
    QList<EncryptedRepoInfo> enc_repo_infos;
    for (size_t i = 0; i < json_array_size(json); i++) {
        EncryptedRepoInfo enc_repo_info = fromJSON(json_array_get(json, i));
        enc_repo_infos.push_back(enc_repo_info);
    }
    return enc_repo_infos;
}


EncryptedReposDialog::EncryptedReposDialog(QWidget *parent) : QDialog(parent)
{

    setWindowTitle(tr("Encrypted Libraries"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    Qt::WindowFlags flags =
            (windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::Dialog) |
            Qt::Window | Qt::WindowSystemMenuHint | Qt::CustomizeWindowHint |
            Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint |
            Qt::WindowMaximizeButtonHint;

    setWindowFlags(flags);

    table_ = new EncryptedReposTableView;
    model_ = new EncryptedReposTableModel(this);
    table_->setModel(model_);

    connect(table_, SIGNAL(sigSetEncRepoPassword(const QString&, const QString&)),
            model_, SLOT(slotSetEncRepoPassword(const QString&, const QString&)));
    connect(table_, SIGNAL(sigClearEncEncRepoPassword(const QString&)),
            model_, SLOT(slotClearEncRepoPassword(const QString&)));

    connect(table_, SIGNAL(sigSetEncRepoPassword(const QString&, const QString&)),
            model_, SLOT(updateEncryptRepoList()));
    connect(table_, SIGNAL(sigClearEncEncRepoPassword(const QString&)),
            model_, SLOT(updateEncryptRepoList()));

    QWidget *widget =  new QWidget(this);
    widget->setObjectName("encryptRepoWidget");

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(widget);
    setLayout(layout);

    createEmptyView();

    QVBoxLayout *vlayout = new QVBoxLayout;
    vlayout->setContentsMargins(1, 0, 1, 0);
    vlayout->setSpacing(0);
    widget->setLayout(vlayout);

    stack_ = new QStackedWidget;
    stack_->insertWidget(INDEX_EMPTY_VIEW, empty_view_);
    stack_->insertWidget(INDEX_TABLE_VIEW, table_);
    stack_->setContentsMargins(0, 0, 0, 0);
    vlayout->addWidget(stack_);

    onModelReset();
    connect(model_, SIGNAL(modelReset()), this, SLOT(onModelReset()));
}

void EncryptedReposDialog::createEmptyView()
{
    empty_view_ = new QWidget(this);

    QVBoxLayout *layout = new QVBoxLayout;
    empty_view_->setLayout(layout);

    QLabel *label = new QLabel;
    label->setText(tr("No Encrypted Library."));
    label->setAlignment(Qt::AlignCenter);

    layout->addWidget(label);
}

void EncryptedReposDialog::onModelReset()
{
    if (model_->rowCount() == 0) {
        stack_->setCurrentIndex(INDEX_EMPTY_VIEW);
    } else {
        stack_->setCurrentIndex(INDEX_TABLE_VIEW);
    }
}

EncryptedReposTableView::EncryptedReposTableView(QWidget *parent)
        : QTableView(parent)
{
    verticalHeader()->hide();
    verticalHeader()->setDefaultSectionSize(36);
    horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setCascadingSectionResizes(true);
    horizontalHeader()->setHighlightSections(false);
    horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    setGridStyle(Qt::NoPen);
    setShowGrid(false);
    setContentsMargins(0, 0, 0, 0);
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setMouseTracking(true);

}

void EncryptedReposTableView::resizeEvent(QResizeEvent *event)
{
    QTableView::resizeEvent(event);
    EncryptedReposTableModel *m = (EncryptedReposTableModel *)(model());
    m->onResize(event->size());
}

void EncryptedReposTableView::onItemDoubleClicked(const QModelIndex& index)
{

}

void EncryptedReposTableView::contextMenuEvent(QContextMenuEvent *event)
{
    QPoint pos = event->pos();
    int row = rowAt(pos.y());
    if (row == -1) {
        return;
    }

    EncryptedReposTableModel *model = (EncryptedReposTableModel *)this->model();

    enc_repo_info_ =  model->encRepoInfoAt(row);

    createContextMenu();
    pos = viewport()->mapToGlobal(pos);
    context_menu_->exec(pos);
    context_menu_->deleteLater();
}

void EncryptedReposTableView::createContextMenu()
{
    context_menu_ = new QMenu(this);
    QAction *sync_status_action_ = new QAction(this);
    if (enc_repo_info_.is_password_set) {
       sync_status_action_->setText(tr("Unsync"));
    } else {
       sync_status_action_->setText(tr("Sync"));
    }
    context_menu_->addAction(sync_status_action_);
    connect(sync_status_action_, SIGNAL(triggered()), this, SLOT(onClickSyncAction()));
}

void EncryptedReposTableView::onClickSyncAction()
{
    bool ok;
    if (enc_repo_info_.is_password_set) {
        ok = gui->yesOrCancelBox(
                tr("After unsyncing, the local encryption key of this library will be cleared and this library cannot be accessed in virtual drive. Are you sure to unsync?"),
                this, true);
        if (ok) {
            emit sigClearEncEncRepoPassword(enc_repo_info_.repo_id);
        }
    } else {
        QString repo_password = QInputDialog::getText(this, QString("Enter library password to sync"),
                                    tr("Enter library password to sync"),
                                    QLineEdit::Password, QString(""), &ok);
        if (ok && !repo_password.isEmpty()) {
            emit sigSetEncRepoPassword(enc_repo_info_.repo_id, repo_password);
        }
    }
}


EncryptedReposTableModel::EncryptedReposTableModel(QObject *parent)
        : QAbstractTableModel(parent),
          repo_name_column_width_(kRepoNameColumnWidth),
          repo_status_column_width_(kRepoStatus)
{
    rpc_client_ = gui->rpcClient();
    updateEncryptRepoList();

}

void EncryptedReposTableModel::updateEncryptRepoList()
{
    json_t *ret;
    if (!rpc_client_->getEncryptedRepoList(&ret)) {
       qWarning("failed to get encrypt library list");
     }
    QList<EncryptedRepoInfo> enc_repo_infos = EncryptedRepoInfo::listFromJSON(ret);
    json_decref(ret);

    beginResetModel();
    enc_repo_infos_ = enc_repo_infos;
    endResetModel();
    return;
}

void EncryptedReposTableModel::slotSetEncRepoPassword(const QString& repo_id, const QString& password)
{
    QString error_msg;
    if (!rpc_client_->setEncryptedRepoPassword(repo_id, password, &error_msg)) {
        if (error_msg.isEmpty()) {
            gui->messageBox(tr("Failed to set encrypted library password"));
        } else if(error_msg == "Wrong password"){
            gui->messageBox(tr("Password error"));
        }
    }
}

void EncryptedReposTableModel::slotClearEncRepoPassword(const QString& repo_id)
{
    if (!rpc_client_->clearEncryptedRepoPassword(repo_id)) {
        gui->messageBox(tr("Failed to clear encrypted library password"));
    }
}

int EncryptedReposTableModel::rowCount(const QModelIndex& parent) const
{
    return enc_repo_infos_.size();
}

int EncryptedReposTableModel::columnCount(const QModelIndex& parent) const
{
    return MAX_COLUMN;
}

void EncryptedReposTableModel::onResize(const QSize &size)
{
    int extra_width = size.width() - kDefaultColumnSum;
    int extra_width_per_column = extra_width / 3;

    repo_name_column_width_ = kRepoNameColumnWidth + extra_width_per_column;
    repo_status_column_width_ = kRepoStatus + extra_width_per_column;

    if (enc_repo_infos_.empty())
        return;

    emit dataChanged(
            index(0, COLUMN_IS_SET_PASSWORD),
            index(enc_repo_infos_.size() - 1 , COLUMN_IS_SET_PASSWORD));
}

QVariant EncryptedReposTableModel::data(const QModelIndex & index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    int column = index.column();

    if (role == Qt::TextAlignmentRole)
        return Qt::AlignLeft + Qt::AlignVCenter;

    if (role == Qt::ToolTipRole)
        return tr("Right click this item to sync or unsync the encrypted library");

    if (role == Qt::SizeHintRole) {
        int h = kDefaultColumnHeight;
        int w = kDefaultColumnWidth;
        switch (column) {
            case COLUMN_REPO_NAME:
                w = repo_name_column_width_;
                break;
            case COLUMN_IS_SET_PASSWORD:
                w = repo_status_column_width_;
                break;
            default:
                break;
        }
        return QSize(w, h);
    }

    if (role != Qt::DisplayRole && role != Qt::DecorationRole) {
        return QVariant();
    }

    const EncryptedRepoInfo &enc_repo_info = enc_repo_infos_[index.row()];

    if (column == COLUMN_REPO_NAME) {
        return enc_repo_info.repo_name ;
    } else if (column == COLUMN_IS_SET_PASSWORD && role == Qt::DecorationRole) {
        if (enc_repo_info.is_password_set) {
            return QIcon(":/images/sync/done.png");
        }
        return QIcon(":/images/sync/cloud.png");
    }

    return QVariant();
}

QVariant EncryptedReposTableModel::headerData(int section,
                                          Qt::Orientation orientation,
                                          int role) const
{
    if (orientation == Qt::Vertical) {
        return QVariant();
    }

    if (role == Qt::TextAlignmentRole)
        return Qt::AlignLeft + Qt::AlignVCenter;

    if (role != Qt::DisplayRole)
        return QVariant();

    if (section == COLUMN_REPO_NAME) {
        return tr("Library");
    } else if (section == COLUMN_IS_SET_PASSWORD) {
        return tr("Sync status");
    }
    return QVariant();
}
