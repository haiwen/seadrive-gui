#include <QtWidgets>
#include <QPixmap>
#include <QTimer>
#include <QCloseEvent>

#include "init-sync-dialog.h"

#include "seadrive-gui.h"
#include "message-poller.h"
#include "ui/tray-icon.h"
#include "utils/utils.h"

namespace
{
const int kCheckDownloadInterval = 1000;
const char* kExplorerPath = "c:/windows/explorer.exe";

} // namespace


InitSyncDialog::InitSyncDialog()
    : QDialog(), new_login_(false), poller_connected_(false)
{
    setupUi(this);
    mLogo->setPixmap(QPixmap(":/images/seafile-32.png"));
    setWindowTitle(tr("Download files list"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    connect(mFinishBtn, SIGNAL(clicked()), this, SLOT(hide()));
    connect(mRunInBackgroundBtn, SIGNAL(clicked()), this, SLOT(hide()));

    check_download_timer_ = new QTimer(this);
    connect(check_download_timer_, SIGNAL(timeout()), this, SLOT(checkDownloadProgress()));
}

void InitSyncDialog::markNewLogin()
{
    new_login_ = true;
}

bool InitSyncDialog::hasNewLogin()
{
    return new_login_;
}

void InitSyncDialog::launch(const QString& domain_id)
{
    if (!poller_connected_) {
        connect(gui->messagePoller(domain_id), SIGNAL(seadriveFSLoaded()),
                this, SLOT(onFSLoaded()));
        poller_connected_ = true;
    }

    gui->trayIcon()->setLoginActionEnabled(false);

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

void InitSyncDialog::finish()
{
    new_login_ = false;

    gui->trayIcon()->setLoginActionEnabled(true);

    QString msg = tr("%1 has dowloaded your files list.").arg(getBrand());
    setStatusText(msg);
    setStatusIcon(":/images/ok-48.png");

    mFinishBtn->setVisible(true);
    mRunInBackgroundBtn->setVisible(false);

    ensureVisible();
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
