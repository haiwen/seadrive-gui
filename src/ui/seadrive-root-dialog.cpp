#include <QtWidgets>
#include <QDir>

#include "seadrive-root-dialog.h"
#include "utils/file-utils.h"
#include "seadrive-gui.h"
#include "utils/utils.h"


namespace {
    const char* kSeadriveRootFolderName = "seadrive_root";
}

SeaDriveRootDialog::SeaDriveRootDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    mLogo->setPixmap(QPixmap(":/images/seafile-32.png"));
    mTitle->setText(tr("Choose %1 Cache Folder ").arg(getBrand()));
    setWindowTitle(tr("Choose %1 Cache Folder").arg(getBrand()));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) |
                   Qt::WindowStaysOnTopHint);
    mCacheDirLineEdit->setText(pathJoin(QDir::homePath(), kSeadriveRootFolderName));

    connect(mOkBtn, SIGNAL(clicked()), this, SLOT(onOkBtnClicked()));
    connect(mSelectSeadriveRootButton, SIGNAL(clicked()), this, SLOT(onSelectSeadriveRootButtonClicked()));
}

void SeaDriveRootDialog::onSelectSeadriveRootButtonClicked()
{
    QString home_path = QDir::homePath();
    QString dir = QFileDialog::getExistingDirectory(this, tr("Please choose %1 cache folder").arg(getBrand()),
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
        if (seadrive_root_ == pathJoin(home_path, kSeadriveRootFolderName)) {
            QDir home_dir(home_path);
            if (!home_dir.exists(kSeadriveRootFolderName)) {
                if (!home_dir.mkdir(kSeadriveRootFolderName)) {
                    gui->errorAndExit(tr("Create %1 folder failed!").arg(kSeadriveRootFolderName));
                }
            }
        }

        QDir dir(seadrive_root_);
        if (dir.exists()) {
            accept();
        }
    }
}
