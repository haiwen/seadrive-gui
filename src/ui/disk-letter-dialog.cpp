#include <QtWidgets>
#include <QPixmap>
#include <QTimer>
#include <QCloseEvent>

#include "rpc/rpc-client.h"
#include "seadrive-gui.h"
#include "utils/utils.h"
#include "utils/utils-win.h"
#include "message-poller.h"

#include "disk-letter-dialog.h"

namespace
{


} // namespace


DiskLetterDialog::DiskLetterDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    mLogo->setPixmap(QPixmap(":/images/seafile-32.png"));
    setWindowTitle(tr("Choose the disk letter for SeaDrive"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) |
                   Qt::WindowStaysOnTopHint);

    mDiskLetter->clear();

    QStringList letters = utils::win::getAvailableDiskLetters();
    int i = 0;
    foreach (const QString& letter, letters) {
        mDiskLetter->addItem(letter);
        if (letter == "S") {
            mDiskLetter->setCurrentIndex(i);
        }
        i++;
    }

    connect(mOkBtn, SIGNAL(clicked()), this, SLOT(onOkBtnClicked()));
}

void DiskLetterDialog::onOkBtnClicked()
{
    disk_letter_ = mDiskLetter->currentText();
    accept();
}
