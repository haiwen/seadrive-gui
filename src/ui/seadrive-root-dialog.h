#ifndef SEAFILE_CLIENT_SEADRIVE_ROOT_DIALOG_H
#define SEAFILE_CLIENT_SEADRIVE_ROOT_DIALOG_H

#include <QDialog>
#include "ui_seadrive-root-dialog.h"

class SeaDriveRootDialog : public QDialog,
                           public Ui::SeaDriveRootDialog
{
    Q_OBJECT
public:
    SeaDriveRootDialog(QWidget *parent=0);

    QString seaDriveRoot() const { return seadrive_root_; }

private slots:
    void onOkBtnClicked();
    void onSelectSeadriveRootButtonClicked();

private:
    Q_DISABLE_COPY(SeaDriveRootDialog)

    QString disk_letter_;
    QString seadrive_root_;
};

#endif // SEAFILE_CLIENT_SEADRIVE_ROOT_DIALOG_H
