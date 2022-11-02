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


InitSyncDialog::InitSyncDialog(const Account &account, QWidget *parent)
    : QDialog(parent), account_(account), finished_(false)
{
    setupUi(this);
    mLogo->setPixmap(QPixmap(":/images/seafile-32.png"));
    setWindowTitle(tr("Download files list"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    waiting_text_ = tr("%1 is fetching the files list, please wait").arg(getBrand());
    dots_ = 0;
    mStatusText->setText(waiting_text_);

    setStatusIcon(":/images/download-48.png");

    mFinishBtn->setVisible(false);

    connect(mRunInBackgroundBtn, SIGNAL(clicked()), this, SLOT(hide()));
    mRunInBackgroundBtn->setVisible(true);

    check_download_timer_ = new QTimer(this);
    connect(check_download_timer_, SIGNAL(timeout()), this, SLOT(checkDownloadProgress()));
    check_download_timer_->start(kCheckDownloadInterval);

    connect(gui->messagePoller(), SIGNAL(seadriveFSLoaded()),
            this, SLOT(onFSLoaded()));
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
    finished_ = true;

    setAttribute(Qt::WA_DeleteOnClose);
    mRunInBackgroundBtn->setVisible(false);

    ensureVisible();

    QString msg =
        tr("%1 has dowloaded your files list.").arg(getBrand());
    setStatusText(msg);
    setStatusIcon(":/images/ok-48.png");

    connect(mFinishBtn, SIGNAL(clicked()), this, SLOT(accept()));
    mFinishBtn->setVisible(true);
}

void InitSyncDialog::fail(const QString &reason)
{
    finished_ = true;
    setAttribute(Qt::WA_DeleteOnClose);
    mRunInBackgroundBtn->setVisible(false);

    ensureVisible();

    setStatusText(reason);
    mFinishBtn->setVisible(true);

    connect(mFinishBtn, SIGNAL(clicked()), this, SLOT(reject()));
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
    if (!finished_) {
        event->ignore();
        hide();
    } else {
        // We only set WA_DeleteOnClose when the dialog is ready to be closed.
        setAttribute(Qt::WA_DeleteOnClose);
        QDialog::closeEvent(event);
    }
}
