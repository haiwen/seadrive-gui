#ifndef SEAFILE_CLIENT_DISK_LETTER_DIALOG_H
#define SEAFILE_CLIENT_DISK_LETTER_DIALOG_H

#include <QDialog>
#include "ui_disk-letter-dialog.h"

class DiskLetterDialog : public QDialog,
                         public Ui::DiskLetterDialog
{
    Q_OBJECT
public:
    DiskLetterDialog(QWidget *parent=0);

    QString diskLetter() const { return disk_letter_ + ":"; }

private slots:
    void onOkBtnClicked();

private:
    Q_DISABLE_COPY(DiskLetterDialog)

    QString disk_letter_;
};

#endif // SEAFILE_CLIENT_DISK_LETTER_DIALOG_H
