#ifndef SEAFILE_CLIENT_FILE_BROWSER_SHAREDLINK_DIALOG_H
#define SEAFILE_CLIENT_FILE_BROWSER_SHAREDLINK_DIALOG_H
#include <QDialog>
#include "api/requests.h"

class QLineEdit;
class QLabel;
class QComboBox;
class QCheckBox;
class DataManager;

class SharedLinkDialog : public QDialog
{
    Q_OBJECT
public:
    SharedLinkDialog(const ShareLinkInfo& linkInfo,QWidget *parent);

    void delectShareLink(const ShareLinkInfo& linkInfo);
    void onAccountChanged();
    int sharedLinkType(int type);
    QString shareLinkTime(qint64 expireTime);
    void reShareFileLinkDirentSuccess(const ShareLinkInfo& link);
    void reShareFileLinkDirentFailed(const ApiError&);
private slots:
    void onCopyText();
    void onDownloadStateChanged(int state);
    void oncurrentShareIndexChanged();
    void onReShareFileLinkDirentShareSuccess(const ShareLinkInfo& link);
    void onReShareFileLinkDirentFailed(const ApiError& error);

    void unShareFileDirentSuccess(const ShareLinkInfo& link);
private:
    const QString text_;
    ShareLinkInfo linkInfo_;
    Account account_;
    DataManager *data_mgr_;

    QLineEdit *editor_;
    QLabel    *copySuccess_label_;
    QLabel    *linkValidity_label_;
    QLabel    *linkTime_label_;
    QComboBox *commboBox_;
    QCheckBox *is_download_checked_;
    QLineEdit *passwordEditor_;
    QPushButton *copy_to_;
    bool password;

};

#endif
