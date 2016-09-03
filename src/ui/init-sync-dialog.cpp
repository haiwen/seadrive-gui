#include <QtWidgets>
#include <QPixmap>
#include <QTimer>
#include <QCloseEvent>

#include "rpc/rpc-client.h"
#include "seadrive-gui.h"
#include "utils/utils.h"

#include "init-sync-dialog.h"

namespace
{
const int kCheckDownloadInterval = 2000;

} // namespace


InitSyncDialog::InitSyncDialog(const Account &account, QWidget *parent)
    : QDialog(parent), account_(account), finished_(false)
{
    setupUi(this);
    mLogo->setPixmap(QPixmap(":/images/seafile-32.png"));
    setWindowTitle(tr("Download files list"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) |
                   Qt::WindowStaysOnTopHint);

    mStatusText->setText(
        tr("%1 is fetching the files list, please wait ...").arg(getBrand()));
    setStatusIcon(":/images/download-48.png");

    mFinishBtn->setVisible(false);

    connect(mRunInBackgroundBtn, SIGNAL(clicked()), this, SLOT(hide()));
    mRunInBackgroundBtn->setVisible(true);

    check_download_timer_ = new QTimer(this);
    connect(check_download_timer_, SIGNAL(timeout()), this, SLOT(checkDownloadProgress()));
    check_download_timer_->start(kCheckDownloadInterval);
}

void InitSyncDialog::checkDownloadProgress()
{
    // TODO: Check the initial sync status via seadrive RPC
    static int checked = 0;
    if (++checked == 5) {
        checked = 0;
        finish();
        check_download_timer_->stop();
    }
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
        tr("%1 has dowloaded your files list.\n"
           "Please click the \"Finish\" button to open %1 folder.").arg(getBrand());
    setStatusText(msg);
    setStatusIcon(":/images/ok-48.png");

    connect(mFinishBtn, SIGNAL(clicked()), this, SLOT(openMountPointAndCloseDialog()));
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
