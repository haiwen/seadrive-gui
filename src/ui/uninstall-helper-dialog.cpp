#include <QtGlobal>

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QtWidgets>
#else
#include <QtGui>
#endif

#include <QIcon>
#include <QMessageBox>
#include <QMainWindow>
#include <QFile>
#include <QFileInfo>

#include "utils/utils.h"
#include "seadrive-gui.h"
#include "utils/uninstall-helpers.h"

#include "uninstall-helper-dialog.h"


UninstallHelperDialog::UninstallHelperDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowTitle(tr("Uninstall %1").arg(getBrand()));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    mText->setText(tr("Do you want to remove the %1 account information?").arg(getBrand()));

    loadQss("qt.css") || loadQss(":/qt.css");
#if defined(Q_OS_WIN32)
    loadQss("qt-win.css") || loadQss(":/qt-win.css");
#elif defined(Q_OS_LINUX)
    loadQss("qt-linux.css") || loadQss(":/qt-linux.css");
#else
    loadQss("qt-mac.css") || loadQss(":/qt-mac.css");
#endif

    const QRect screen = QApplication::desktop()->screenGeometry();
    move(screen.center() - this->rect().center());

    connect(mYesBtn, SIGNAL(clicked()),
            this, SLOT(onYesClicked()));

    connect(mNoBtn, SIGNAL(clicked()),
            this, SLOT(doExit()));
}

void UninstallHelperDialog::onYesClicked()
{
    mYesBtn->setEnabled(false);
    mNoBtn->setEnabled(false);
    mText->setText(tr("Removing account information..."));

    RemoveSeafileDataThread *thread = new RemoveSeafileDataThread;
    thread->start();
    connect(thread, SIGNAL(finished()), this, SLOT(doExit()));
}

void UninstallHelperDialog::doExit()
{
    QCoreApplication::exit(0);
}

bool UninstallHelperDialog::loadQss(const QString& path)
{
    QFile file(path);
    if (!QFileInfo(file).exists()) {
        return false;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream input(&file);
    style_ += "\n";
    style_ += input.readAll();
    qApp->setStyleSheet(style_);

    return true;
}

void RemoveSeafileDataThread::run()
{
    QString seadrive_dir;
    QString seadrive_data_dir;

    if (get_seadrive_dir(&seadrive_dir) < 0) {
        fprintf(stderr, "seadrive directory not found");
        return;
    }

    if (get_seadrive_data_dir(seadrive_dir, &seadrive_data_dir) < 0) {
        delete_dir_recursively(seadrive_dir);
        return;
    }

}
