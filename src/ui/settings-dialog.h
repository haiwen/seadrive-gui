#include <QDialog>
#include "ui_settings-dialog.h"

#include <QUrl>
#include <QString>


class SettingsDialog : public QDialog,
                    public Ui::SettingsDialog
{
    Q_OBJECT
public:
    SettingsDialog(QWidget *parent=0);

private slots:

    void closeEvent(QCloseEvent *event);
    void showEvent(QShowEvent *event);
    void updateSettings();
    void onOkBtnClicked();
    void selectDirAction();

    void proxyRequirePasswordChanged(int state);
    void showHideControlsBasedOnCurrentProxyType(int state);

private:
    bool updateProxySettings();
    bool validateProxyInputs();

    QString preferred_disk_letter_;
    QString current_seadrive_root_;
    QString current_cache_dir_;
    bool current_session_access_;

    Q_DISABLE_COPY(SettingsDialog);
};
