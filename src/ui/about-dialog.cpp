#include <QtWidgets>

#include "seadrive-gui.h"
#include "utils/utils.h"

#include "about-dialog.h"

#if HAVE_SPARKLE_SUPPORT
#include "auto-update-service.h"

namespace {

} // namespace
#endif

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
{
    setupUi(this);
    setWindowTitle(tr("About %1").arg(getBrand()));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) |
                   Qt::WindowStaysOnTopHint);

    version_text_ = tr("<h4>Seafile Drive Client %2</h4>")
	               .arg(STRINGIZE(SEADRIVE_GUI_VERSION))
#if defined(SEAFILE_CLIENT_REVISION)
                       .append("<h5> REV %1 </h5>")
                       .arg(STRINGIZE(SEAFILE_CLIENT_REVISION))
#endif
		       ;
    mVersionText->setText(version_text_);

    connect(mOKBtn, SIGNAL(clicked()), this, SLOT(close()));

#if HAVE_SPARKLE_SUPPORT
    mCheckUpdateBtn->setVisible(true);
    connect(mCheckUpdateBtn, SIGNAL(clicked()), this, SLOT(checkUpdate()));
#else
    mCheckUpdateBtn->setVisible(false);
#endif
}

#if HAVE_SPARKLE_SUPPORT
void AboutDialog::checkUpdate()
{
    AutoUpdateService::instance()->setRegistryPath();
    AutoUpdateService::instance()->setRequestParams();
    AutoUpdateService::instance()->checkUpdate();
}
#endif
