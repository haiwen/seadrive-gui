#include <QtWidgets>
#include <QDir>

#include "seadrive-root-dialog.h"


SeaDriveRootDialog::SeaDriveRootDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    mLogo->setPixmap(QPixmap(":/images/seafile-32.png"));
    setWindowTitle(tr("Select SeaDrive cache folder"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) |
                   Qt::WindowStaysOnTopHint);

    connect(mOkBtn, SIGNAL(clicked()), this, SLOT(onOkBtnClicked()));
    connect(mSelectSeadriveRootButton, SIGNAL(clicked()), this, SLOT(onSelectSeadriveRootButtonClicked()));
}

void SeaDriveRootDialog::onSelectSeadriveRootButtonClicked()
{
    QString home_path = QDir::homePath();
    QString dir = QFileDialog::getExistingDirectory(this, tr("Please choose seadrive cache folder"),
                                                    home_path,
                                                    QFileDialog::ShowDirsOnly
                                                    | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty())
        return;

    QString text = dir;
    if (text.endsWith("/")) {
        text.resize(text.size() - 1);
    }
    mCacheDirLineEdit->setText(text);
    selected_path_ = text;

}

void SeaDriveRootDialog::onOkBtnClicked()
{
    seadrive_root_ = selected_path_;
    accept();
}
