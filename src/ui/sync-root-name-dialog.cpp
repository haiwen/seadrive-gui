#include "ui/sync-root-name-dialog.h"

#include <QDir>
#include <QRegularExpression>
#include "seadrive-gui.h"

SyncRootNameDialog::SyncRootNameDialog(QString name, bool allow_existing_default, QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    setWindowTitle(tr("Sync Root Folder Name"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    default_name_ = name;
    allow_existing_default_ = allow_existing_default;

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

    QDir seadrive_root(gui->seadriveRoot());
    // A recovered legacy root already exists by design; every other existing
    // root name would conflict with another account.
    const bool is_existing_legacy_name = allow_existing_default_ && name == default_name_;
    if (seadrive_root.exists(name) && !is_existing_legacy_name) {
        gui->warningBox(tr("A sync root folder with this name already exists."), this);
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
