#ifndef SEAFILE_CLIENT_FILE_BROWSER_ADVANCEDSHAREDLINK_DIALOG_H
#define SEAFILE_CLIENT_FILE_BROWSER_ADVANCEDSHAREDLINK_DIALOG_H
#include <QDialog>

class QLineEdit;
class AdvancedSharedLinkDialog : public QDialog
{
    Q_OBJECT
public:
    AdvancedSharedLinkDialog(const QString &text, QWidget *parent);

private slots:
    void onCopyText();
    void onDownloadStateChanged(int state);
private:
    const QString text_;
    QLineEdit *editor_;
};

#endif
