#include <QtWidgets>

#include "seadrive-gui.h"
#include "utils/utils.h"
#include "utils/i18n-utils.h"
#include "utils/utils-win.h"
#include "utils/file-utils.h"

#include "seadrive-root-dialog.h"


SeaDriveRootDialog::SeaDriveRootDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    mLogo->setPixmap(QPixmap(":/images/seafile-32.png"));
    setWindowTitle(tr("Select seadrive root directory"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) |
                   Qt::WindowStaysOnTopHint);

    mDiskLetter->clear();

    QSet<QString> disk_letters_set = utils::win::getUsedDiskLetters();
    int i = 0;
    QList<QString> disk_letters = disk_letters_set.values();
    foreach(const QString letter, disk_letters) {
        mDiskLetter->addItem(letter);
        if (letter == "C") {
            mDiskLetter->setCurrentIndex(i);
        }
        i++;
    }

    connect(mOkBtn, SIGNAL(clicked()), this, SLOT(onOkBtnClicked()));
    connect(mSelectSeadriveRootButton, SIGNAL(clicked()), this, SLOT(onSelectSeadriveRootButtonClicked()));
}

void SeaDriveRootDialog::onSelectSeadriveRootButtonClicked()
{
    QString disk_letter = mDiskLetter->currentText();
    QString dir = QFileDialog::getExistingDirectory(this, tr("Please choose the cache folder"),
                                                    QString("%1://").arg(disk_letter),
                                                    QFileDialog::ShowDirsOnly
                                                    | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty())
        return;
    //setDirectoryText(dir);
    QString text = dir.mid(3);
    if (text.endsWith("/")) {
        text.resize(text.size() - 1);
    }
    mCacheDirLineEdit->setText(text);
    seadrive_root_ =  disk_letter + ":/" + text;

}

void SeaDriveRootDialog::onOkBtnClicked()
{
    accept();
}
