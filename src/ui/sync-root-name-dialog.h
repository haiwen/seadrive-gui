#ifndef SEAFILE_CLIENT_SYNC_ROOT_NAME_DIALOG_H
#define SEAFILE_CLIENT_SYNC_ROOT_NAME_DIALOG_H

#include <QDialog>
#include "ui_sync-root-name-dialog.h"

class SyncRootNameDialog : public QDialog,
                           public Ui::SyncRootNameDialog
{
    Q_OBJECT

public:
    SyncRootNameDialog(QString name, QWidget *parent = 0);

    QString customName() const { return custom_name_; }

    void accept() override;

private slots:
    void onUseDefaultNameToggled(bool checked);

private:
    QString default_name_;
    QString custom_name_;
};

#endif
