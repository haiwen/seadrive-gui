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
    InitSyncDialog(const Account& account, QWidget *parent=0);
    void ensureVisible();

private slots:
    void closeEvent(QCloseEvent *event);
    void checkDownloadProgress();
    void openMountPointAndCloseDialog();
    void onFSLoaded();

private:
    Q_DISABLE_COPY(InitSyncDialog)

    void setStatusText(const QString& status);
    void setStatusIcon(const QString& path);
    void finish();
    void fail(const QString& reason);

    Account account_;

    QTimer *check_download_timer_;

    bool finished_;

    QString waiting_text_;
    int dots_;
};

#endif // SEAFILE_CLIENT_INIT_VDRIVE_DIALOG_H
