#include <QtWidgets>
#include <QDir>

#include "seadrive-root-dialog.h"
#include "utils/file-utils.h"
#include "seadrive-gui.h"


SeaDriveRootDialog::SeaDriveRootDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    mLogo->setPixmap(QPixmap(":/images/seafile-32.png"));
    setWindowTitle(tr("Choose SeaDrive Cache Folder"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) |
                   Qt::WindowStaysOnTopHint);
    mCacheDirLineEdit->setText(pathJoin(QDir::homePath(), "seadrive_root"));

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

}

void SeaDriveRootDialog::onOkBtnClicked()
{
    seadrive_root_ = mCacheDirLineEdit->text();
    QString home_path = QDir::homePath();

    if (!seadrive_root_.isEmpty()) {
        if (seadrive_root_ == pathJoin(home_path, "seadrive_root")) {
            QDir home_dir(home_path);
            if (!home_dir.exists("seadrive_root")) {
                if (!home_dir.mkdir("seadrive_root")) {
                    gui->errorAndExit(tr("Create seadrive_root folder failed!"));
                }
            }
        }

        QDir dir(seadrive_root_);
        if (dir.exists()) {
            accept();
        }
    }
}
