
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QLabel>

#include "ui/encrypt-repo-setting-dialog.h"
#include "encrypt-library-dialog.h"

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
    enc_repo_info.is_set_password  = json.getBool("is_passwd_set");
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


EncryptedRepoDialog::EncryptedRepoDialog(QWidget *parent) : QDialog(parent)
{

    setWindowTitle(tr("Encrypted Repository List"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    Qt::WindowFlags flags =
            (windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::Dialog) |
            Qt::Window | Qt::WindowSystemMenuHint | Qt::CustomizeWindowHint |
            Qt::WindowMinimizeButtonHint | Qt::WindowCloseButtonHint |
            Qt::WindowMaximizeButtonHint;

    setWindowFlags(flags);

    table_ = new EncryptRepoTableView;
    model_ = new EncryptRepoTableModel(this);
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

void EncryptedRepoDialog::createEmptyView()
{
    empty_view_ = new QWidget(this);

    QVBoxLayout *layout = new QVBoxLayout;
    empty_view_->setLayout(layout);

    QLabel *label = new QLabel;
    label->setText(tr("No Encrypted Repo."));
    label->setAlignment(Qt::AlignCenter);

    layout->addWidget(label);
}

void EncryptedRepoDialog::onModelReset()
{
    if (model_->rowCount() == 0) {
        stack_->setCurrentIndex(INDEX_EMPTY_VIEW);
    } else {
        stack_->setCurrentIndex(INDEX_TABLE_VIEW);
    }
}

EncryptRepoTableView::EncryptRepoTableView(QWidget *parent)
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

    connect(this, SIGNAL(doubleClicked(const QModelIndex&)),
            this, SLOT(onItemDoubleClicked(const QModelIndex&)));
}

void EncryptRepoTableView::resizeEvent(QResizeEvent *event)
{
    QTableView::resizeEvent(event);
    EncryptRepoTableModel *m = (EncryptRepoTableModel *)(model());
    m->onResize(event->size());
}

void EncryptRepoTableView::onItemDoubleClicked(const QModelIndex& index)
{
    EncryptRepoTableModel *model = (EncryptRepoTableModel *)this->model();
    EncryptedRepoInfo enc_repo_info = model->encRepoInfoAt(index.row());
    if (enc_repo_info.is_set_password) {
        EncryptRepoSetting dialog(false, this);
        if (dialog.exec() == QDialog::Accepted) {
            emit sigClearEncEncRepoPassword(enc_repo_info.repo_id);
        }
    } else {
        EncryptRepoSetting dialog(true, this);
        if (dialog.exec() == QDialog::Accepted) {
            emit sigSetEncRepoPassword(enc_repo_info.repo_id, dialog.getRepoPassword());
        }
    }
}


EncryptRepoTableModel::EncryptRepoTableModel(QObject *parent)
        : QAbstractTableModel(parent),
          repo_name_column_width_(kRepoNameColumnWidth),
          repo_status_column_width_(kRepoStatus)
{
    rpc_client_ = new SeafileRpcClient();
    rpc_client_ ->connectDaemon();

    updateEncryptRepoList();

}

void EncryptRepoTableModel::updateEncryptRepoList()
{
    json_t *ret;
    if (!rpc_client_->getEncryptedRepoList(&ret)) {
       qWarning("falild get encrypt library list");
     }
    QList<EncryptedRepoInfo> enc_repo_infos = EncryptedRepoInfo::listFromJSON(ret);
    json_decref(ret);

    beginResetModel();
    enc_repo_infos_ = enc_repo_infos;
    endResetModel();
    return;
}

void EncryptRepoTableModel::slotSetEncRepoPassword(const QString& repo_id, const QString& password) {
    if (!rpc_client_->setEncryptedRepoPassword(repo_id, password)) {
        gui->messageBox(tr("Failed to set encrypted repository password"));
    }
}

void EncryptRepoTableModel::slotClearEncRepoPassword(const QString& repo_id) {
    if (!rpc_client_->clearEncryptedRepoPassword(repo_id)) {
        gui->messageBox(tr("Failed to clear encrypted repository password"));
    }
}

int EncryptRepoTableModel::rowCount(const QModelIndex& parent) const
{
    return enc_repo_infos_.size();
}

int EncryptRepoTableModel::columnCount(const QModelIndex& parent) const
{
    return MAX_COLUMN;
}

void EncryptRepoTableModel::onResize(const QSize &size)
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

QVariant EncryptRepoTableModel::data(const QModelIndex & index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    int column = index.column();

    if (role == Qt::TextAlignmentRole)
        return Qt::AlignLeft + Qt::AlignVCenter;

    if (role == Qt::ToolTipRole)
        return tr("Double click to set password or unset password");

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
        if (enc_repo_info.is_set_password) {
            return QIcon(":/images/sync/done.png");
        }
        return QIcon(":/images/sync/cloud.png");
    }

    return QVariant();
}

QVariant EncryptRepoTableModel::headerData(int section,
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
        return tr("Repository");
    } else if (section == COLUMN_IS_SET_PASSWORD) {
        return tr("Sync status");
    }
    return QVariant();
}
