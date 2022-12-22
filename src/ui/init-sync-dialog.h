#ifndef SEAFILE_CLIENT_INIT_VDRIVE_DIALOG_H
#define SEAFILE_CLIENT_INIT_VDRIVE_DIALOG_H

#include <QDialog>
#include "ui_init-sync-dialog.h"
#include "account.h"
#include "api/requests.h"

class QTimer;
class QCloseEvent;

class InitSyncDialog : public QDialog,
                       public Ui::InitSyncDialog
{
    Q_OBJECT

public:
    InitSyncDialog();

    void markNewLogin();
    bool hasNewLogin();
    void launch();

private slots:
    void checkDownloadProgress();
    void onFSLoaded();
    void closeEvent(QCloseEvent *event);

private:
    Q_DISABLE_COPY(InitSyncDialog)

    void finish();
    void setStatusText(const QString& status);
    void setStatusIcon(const QString& path);
    void ensureVisible();

    QTimer *check_download_timer_;

    bool new_login_;
    bool poller_connected_;

    QString waiting_text_;
    int dots_;
};

#endif // SEAFILE_CLIENT_INIT_VDRIVE_DIALOG_H
