#ifndef SEADRIVE_GUI_ENCRYPTREPOSETTING_H
#define SEADRIVE_GUI_ENCRYPTREPOSETTING_H
#include <QDialog>

class QLineEdit;
class EncryptRepoSetting : public QDialog
{
    Q_OBJECT
public:
    EncryptRepoSetting(const bool is_settings_password, QWidget *parent = NULL);

    QString getRepoPassword();

private slots:
    void slotSetPassword();

private:
    QString password_;
    QLineEdit *repo_password_line_edit_;

};


#endif //SEADRIVE_GUI_ENCRYPTREPOSETTING_H
