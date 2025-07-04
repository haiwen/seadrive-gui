#include "ui/sync-root-name-dialog.h"

#include <QRegularExpression>
#include "seadrive-gui.h"

SyncRootNameDialog::SyncRootNameDialog(QString name, QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    setWindowTitle(tr("Sync Root Folder Name"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    default_name_ = name;

    mUseDefaultName->setChecked(true);
    onUseDefaultNameToggled(true);

    connect(mUseDefaultName, SIGNAL(toggled(bool)),
            this, SLOT(onUseDefaultNameToggled(bool)));

    connect(mOkBtn, SIGNAL(clicked()), this, SLOT(accept()));
}

void SyncRootNameDialog::accept()
{
    QString name = mCustomName->text().trimmed();

    // remove trailing dots
    while (name.endsWith(".")) {
        name.resize(name.size() - 1);
    }

    // validate the name
    if (name.isEmpty()) {
        gui->warningBox(tr("Sync root name cannot be empty."), this);
        return;
    }
    if (name.contains(QRegularExpression("[<>:\"/\\\\|?*]"))) {
        gui->warningBox(tr("Sync root name cannot contain the following characters: < > : \" / \\ | ? *"), this);
        return;
    }

    custom_name_ = name;

    QDialog::accept();
}

void SyncRootNameDialog::onUseDefaultNameToggled(bool checked)
{
    mCustomName->setEnabled(!checked);
    if (checked) {
        mCustomName->setText(default_name_);
    }
}
