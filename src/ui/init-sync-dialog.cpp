#include <QtWidgets>
#include <QPixmap>
#include <QTimer>
#include <QCloseEvent>

#include "rpc/rpc-client.h"
#include "seadrive-gui.h"
#include "utils/utils.h"
#include "message-poller.h"

#include "init-sync-dialog.h"

namespace
{
const int kCheckDownloadInterval = 1000;
const char* kExplorerPath = "c:/windows/explorer.exe";

} // namespace


InitSyncDialog::InitSyncDialog()
    : QDialog(), prepared_(false), finished_(false), poller_connected_(false)
{
    setupUi(this);
    mLogo->setPixmap(QPixmap(":/images/seafile-32.png"));
    setWindowTitle(tr("Download files list"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    connect(mRunInBackgroundBtn, SIGNAL(clicked()), this, SLOT(hide()));

    check_download_timer_ = new QTimer(this);
    connect(check_download_timer_, SIGNAL(timeout()), this, SLOT(checkDownloadProgress()));

}

void InitSyncDialog::prepare(const Account& account)
{
    prepared_ = true;
    finished_ = false;
    account_ = account;
}

void InitSyncDialog::start()
{
    if (!prepared_) {
        return;
    }

    if (!poller_connected_) {
        connect(gui->messagePoller(), SIGNAL(seadriveFSLoaded()),
                this, SLOT(onFSLoaded()));
        poller_connected_ = true;
    }

    waiting_text_ = tr("%1 is fetching the files list, please wait").arg(getBrand());
    setStatusText(waiting_text_);
    setStatusIcon(":/images/download-48.png");

    mFinishBtn->setVisible(false);
    mRunInBackgroundBtn->setVisible(true);

    dots_ = 0;
    check_download_timer_->start(kCheckDownloadInterval);

    ensureVisible();
}

void InitSyncDialog::checkDownloadProgress()
{
    dots_ = (dots_ + 1) % 4;
    QString text = waiting_text_ + " ";
    for (int i = 0; i < dots_; i++) {
        text += ".";
    }
    mStatusText->setText(text);
}

void InitSyncDialog::onFSLoaded()
{
    check_download_timer_->stop();
    finish();
}

void InitSyncDialog::openMountPointAndCloseDialog()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(gui->mountDir()));
    accept();
}

void InitSyncDialog::finish()
{
    prepared_ = false;
    finished_ = true;

    setAttribute(Qt::WA_DeleteOnClose);
    mRunInBackgroundBtn->setVisible(false);

    ensureVisible();

    QString msg =
        tr("%1 has dowloaded your files list.").arg(getBrand());
    setStatusText(msg);
    setStatusIcon(":/images/ok-48.png");

    connect(mFinishBtn, SIGNAL(clicked()), this, SLOT(hide()));
    mFinishBtn->setVisible(true);
}

void InitSyncDialog::fail(const QString &reason)
{
    prepared_ = false;
    finished_ = true;
    setAttribute(Qt::WA_DeleteOnClose);
    mRunInBackgroundBtn->setVisible(false);

    ensureVisible();

    setStatusText(reason);

    connect(mFinishBtn, SIGNAL(clicked()), this, SLOT(hide()));
    mFinishBtn->setVisible(true);
}

void InitSyncDialog::setStatusText(const QString &status)
{
    mStatusText->setText(status);
}

void InitSyncDialog::setStatusIcon(const QString &path)
{
    mStatusIcon->setPixmap(QPixmap(path));
}

void InitSyncDialog::ensureVisible()
{
    show();
    raise();
    activateWindow();
}

void InitSyncDialog::closeEvent(QCloseEvent *event)
{
    event->ignore();
    hide();
}
