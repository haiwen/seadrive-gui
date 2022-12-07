#include <QtGlobal>

#include <QtWidgets>
#include <QDebug>
#include <QSettings>

#include "i18n.h"
#include "account-mgr.h"
#include "utils/utils.h"
#include "utils/utils-win.h"
#include "seadrive-gui.h"
#include "settings-mgr.h"
#include "api/requests.h"
#include "settings-dialog.h"
#include "rpc/rpc-client.h"
#if defined(_MSC_VER)
#include "utils/registry.h"
#endif

#ifdef HAVE_SPARKLE_SUPPORT
#include "auto-update-service.h"
#endif

namespace {

const char *kSettingsGroupForSettingsDialog = "SettingsDialog";


#ifdef Q_OS_WIN32
// Insert the current disk letter into the proper position of the available disk
// letters list.
void insertDiskLetter(QStringList* letters, QString letter_to_insert)
{
    if (letters->contains(letter_to_insert)) {
        return;
    }
    QStringList new_letters;
    int i = 0;
    foreach (const QString &letter, *letters) {
        if (letter > letter_to_insert) {
            break;
        }
        i++;
    }
    letters->insert(i, letter_to_insert);
}
#endif

} // namespace

SettingsDialog::SettingsDialog(QWidget *parent) : QDialog(parent),
    current_cache_dir_(QString()),
    current_session_access_(false)
{
    setupUi(this);
    setWindowTitle(tr("Settings"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);


    mAutoStartCheckBox->setText(
        tr("Auto start %1 after login").arg(getBrand()));

    mTabWidget->setCurrentIndex(0);

#ifdef HAVE_SPARKLE_SUPPORT
    if (!AutoUpdateService::instance()->shouldSupportAutoUpdate()) {
        mCheckLatestVersionBox->setVisible(false);
    }
#endif

    mLanguageComboBox->addItems(I18NHelper::getInstance()->getLanguages());
    SettingsManager mgr;
#if !defined(_MSC_VER)
    if (!mgr.getCacheDir(&current_cache_dir_))
        current_cache_dir_ = QDir(gui->seadriveDataDir()).absolutePath();
    mShowCacheDir->setText(current_cache_dir_);
    mShowCacheDir->setReadOnly(true);
    mCacheLabel->setText(tr("Cache directory:"));
#endif

#if defined(Q_OS_MAC)
    mCacheLabel->hide();
    mShowCacheDir->hide();
    mSelectBtn->hide();
#endif

    mSpotlightCheckBox->setText(tr("Enable search in finder"));

    // The range of mProxyPort is set to (0, 65535) in the ui file, so we
    // don't bother with that here.
    mProxyMethodComboBox->insertItem(SettingsManager::NoProxy, tr("None"));
    mProxyMethodComboBox->insertItem(SettingsManager::HttpProxy, tr("HTTP Proxy"));
    mProxyMethodComboBox->insertItem(SettingsManager::SocksProxy, tr("Socks5 Proxy"));
    mProxyMethodComboBox->insertItem(SettingsManager::SystemProxy, tr("System Proxy"));
    connect(mProxyMethodComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(showHideControlsBasedOnCurrentProxyType(int)));
    connect(mProxyRequirePassword, SIGNAL(stateChanged(int)),
            this, SLOT(proxyRequirePasswordChanged(int)));

#if defined(Q_OS_MAC)
    layout()->setContentsMargins(8, 9, 9, 4);
    layout()->setSpacing(5);

    mDownloadSpinBox->setAttribute(Qt::WA_MacShowFocusRect, 0);
    mUploadSpinBox->setAttribute(Qt::WA_MacShowFocusRect, 0);
#endif

    connect(mSelectBtn, SIGNAL(clicked()), this, SLOT(selectDirAction()));
    connect(mOkBtn, SIGNAL(clicked()), this, SLOT(onOkBtnClicked()));
    adjustSize();

    mSpotlightCheckBox->hide();
}

void SettingsDialog::updateSettings()
{
    SettingsManager *mgr = gui->settingsManager();
    bool language_changed = false;
    bool seadrive_root_changed = false;
    bool cache_dir_changed = false;
    bool diskLetter_changed = false;
    bool current_access_changed = false;
    QString spotlight_updated;

    if (mBasicTab->isEnabled()) {
        mgr->setNotify(mNotifyCheckBox->checkState() == Qt::Checked);
        mgr->setAutoStart(mAutoStartCheckBox->checkState() == Qt::Checked);
        mgr->setSyncExtraTempFile(mSyncExtraTempFileCheckBox->checkState() == Qt::Checked);

        mgr->setMaxDownloadRatio(mDownloadSpinBox->value());
        mgr->setMaxUploadRatio(mUploadSpinBox->value());

        mgr->setHttpSyncCertVerifyDisabled(mDisableVerifyHttpSyncCert->checkState() == Qt::Checked);

        if ((mSpotlightCheckBox->checkState() == Qt::Checked) != mgr->getSearchEnabled()) {
            mgr->setSearchEnabled(mSpotlightCheckBox->checkState() == Qt::Checked);
            spotlight_updated = mSpotlightCheckBox->checkState() == Qt::Checked
                                ? tr("enabled search")
                                : tr("disabled search");
        }

#ifdef Q_OS_WIN32
        mgr->setShellExtensionEnabled(mShellExtCheckBox->checkState() == Qt::Checked);
        if (!preferred_disk_letter_.contains(mDiskLetter->currentText())) {
            mgr->setDiskLetter(mDiskLetter->currentText() + ":");
            diskLetter_changed = true;
        }
#endif

#ifdef HAVE_SPARKLE_SUPPORT
        if (AutoUpdateService::instance()->shouldSupportAutoUpdate()) {
            bool enabled = mCheckLatestVersionBox->checkState() == Qt::Checked;
            AutoUpdateService::instance()->setAutoUpdateEnabled(enabled);
        }
#endif
    }

    if (mAdvancedTab->isEnabled()) {
        mgr->setCacheCleanIntervalMinutes(mCacheCleanInterval->value());
        mgr->setCacheSizeLimitGB(mCacheSizeLimit->value());
#if defined(Q_OS_WIN32)
        bool current_access = mCurrentAccess->checkState() == Qt::Checked;
        if (current_session_access_ != current_access) {
            current_session_access_ = current_access;
            mgr->setCurrentUserAccess(current_access);
            current_access_changed = true;
        }
#endif

#if defined(_MSC_VER)
        if (mShowCacheDir->text() != current_seadrive_root_ ) {
            seadrive_root_changed = true;
            mgr->setSeadriveRoot(mShowCacheDir->text());
        }
        RegElement::removeIconRegItem();
#else
        if (mShowCacheDir->text() != current_cache_dir_) {
            cache_dir_changed = true;
            mgr->setCacheDir(mShowCacheDir->text());
        }
#endif
    }

    if (mLanguageComboBox->currentIndex() != I18NHelper::getInstance()->preferredLanguage()) {
        I18NHelper::getInstance()->setPreferredLanguage(mLanguageComboBox->currentIndex());
        language_changed = true;
    }

    updateProxySettings();

    if (language_changed && gui->yesOrNoBox(
        tr("You have changed language, Restart to apply it?"), this, true)) {
        gui->restartApp();
    }
    if (seadrive_root_changed && gui->yesOrNoBox(
        tr("You have changed %1 cache folder. Restart to apply it?").arg(getBrand()), this, true)) {
        gui->restartApp();
    }
    if (cache_dir_changed && gui->yesOrNoBox(
        tr("You have changed cache directory. Restart to apply it?"), this, true)) {
        gui->restartApp();
    }
    if (!spotlight_updated.isEmpty() && gui->yesOrNoBox(
        tr("You have %1. Restart to apply it?").arg(spotlight_updated), this, true)) {
        gui->restartApp();
    }
    if (diskLetter_changed && gui->yesOrNoBox(
        tr("You have changed disk letter. Restart to apply it?"), this, true)) {
        gui->restartApp();
    }
    if (current_access_changed && gui->yesOrNoBox(
        tr("You have changed drive access option. Restart to apply it?"), this, true)) {
        gui->restartApp();
    }
}

void SettingsDialog::closeEvent(QCloseEvent *event)
{
    // There is only one instance of settings dialog during the applet life
    // time. During startup, applet loads settings from registry (or similar
    // places on linux/osx) and load part of the settings from seaf daemon.
    // Each time a user modifieds a settings item and clicks "OK" button, the
    // change is both updated in memory and persisted to the registry.
    event->ignore();
    this->hide();
}

void SettingsDialog::showEvent(QShowEvent *event)
{
    bool isDaemonUp = gui->rpcClient()->isConnected();
    SettingsManager *mgr = gui->settingsManager();

    if (isDaemonUp) {
        mBasicTab->setDisabled(false);
        mAdvancedTab->setDisabled(false);
    } else {
        mBasicTab->setDisabled(true);
        mAdvancedTab->setDisabled(true);
    }

    if (isDaemonUp) {
        mgr->loadSettings();

        Qt::CheckState state;

        state = mgr->syncExtraTempFile() ? Qt::Checked : Qt::Unchecked;
        mSyncExtraTempFileCheckBox->setCheckState(state);

        state = mgr->httpSyncCertVerifyDisabled() ? Qt::Checked : Qt::Unchecked;
        mDisableVerifyHttpSyncCert->setCheckState(state);

        // currently supports windows only
        state = mgr->autoStart() ? Qt::Checked : Qt::Unchecked;
        mAutoStartCheckBox->setCheckState(state);
#if !defined(Q_OS_WIN32) && !defined(Q_OS_MAC)
        mAutoStartCheckBox->hide();
#endif

#if defined(Q_OS_WIN32)
        state = mgr->shellExtensionEnabled() ? Qt::Checked : Qt::Unchecked;
        mShellExtCheckBox->setCheckState(state);
#else
        mShellExtCheckBox->hide();
#endif
        state = mgr->notify() ? Qt::Checked : Qt::Unchecked;
        mNotifyCheckBox->setCheckState(state);

        int ratio;
        ratio = mgr->maxDownloadRatio();
        mDownloadSpinBox->setValue(ratio);
        ratio = mgr->maxUploadRatio();
        mUploadSpinBox->setValue(ratio);

        int value;
        value = mgr->getCacheCleanIntervalMinutes();
        mCacheCleanInterval->setValue(value);
        value = mgr->getCacheSizeLimitGB();
        mCacheSizeLimit->setValue(value);

#ifdef HAVE_SPARKLE_SUPPORT
        if (AutoUpdateService::instance()->shouldSupportAutoUpdate()) {
            state = AutoUpdateService::instance()->autoUpdateEnabled() ? Qt::Checked : Qt::Unchecked;
            mCheckLatestVersionBox->setCheckState(state);
        }
#endif

#ifdef Q_OS_WIN32
        QStringList letters = utils::win::getAvailableDiskLetters();
        if (!mgr->getDiskLetter(&preferred_disk_letter_)) {
            preferred_disk_letter_ = gui->seadriveRoot();
        }
        preferred_disk_letter_ = preferred_disk_letter_.at(0);
        insertDiskLetter(&letters, gui->seadriveRoot().at(0));

        mDiskLetter->clear();
        int i = 0;
        foreach (const QString& letter, letters) {
            mDiskLetter->addItem(letter);
            if (preferred_disk_letter_.contains(letter)) {
                mDiskLetter->setCurrentIndex(i);
            }
            i++;
        }

        current_session_access_  = mgr->currentUserAccess();
        state = current_session_access_ ? Qt::Checked : Qt::Unchecked;
        mCurrentAccess->setCheckState(state);
#ifdef _MSC_VER
        mDiskLetterLabel->setVisible(false);
        mDiskLetter->setVisible(false);
        mCurrentAccess->setVisible(false);
#endif
#else
        mDiskLetterLabel->setVisible(false);
        mDiskLetter->setVisible(false);
        mCurrentAccess->setVisible(false);
#endif // Q_OS_WIN32

#if defined(_MSC_VER)
        if (!mgr->getSeadriveRoot(&current_seadrive_root_)) {
            current_seadrive_root_ = gui->seadriveRoot();
        }
        mShowCacheDir->setText(current_seadrive_root_);
        mShowCacheDir->setReadOnly(true);
#endif
    }

    mLanguageComboBox->setCurrentIndex(I18NHelper::getInstance()->preferredLanguage());

    mgr->loadProxySettings();
    SettingsManager::SeafileProxy proxy = mgr->getProxy();
    showHideControlsBasedOnCurrentProxyType(proxy.type);
    mProxyMethodComboBox->setCurrentIndex(proxy.type);
    mProxyHost->setText(proxy.host);
    mProxyPort->setValue(proxy.port);
    mProxyUsername->setText(proxy.username);
    mProxyPassword->setText(proxy.password);
    if (!proxy.username.isEmpty())
        mProxyRequirePassword->setChecked(true);

    QDialog::showEvent(event);
}

void SettingsDialog::proxyRequirePasswordChanged(int state)
{
    if (state == Qt::Checked) {
        mProxyUsername->setEnabled(true);
        mProxyUsernameLabel->setEnabled(true);
        mProxyPassword->setEnabled(true);
        mProxyPasswordLabel->setEnabled(true);
    } else {
        mProxyUsername->setEnabled(false);
        mProxyUsernameLabel->setEnabled(false);
        mProxyPassword->setEnabled(false);
        mProxyPasswordLabel->setEnabled(false);
    }
}

void SettingsDialog::showHideControlsBasedOnCurrentProxyType(int state)
{
    SettingsManager::ProxyType proxy_type =
        static_cast<SettingsManager::ProxyType>(state);
    switch(proxy_type) {
        case SettingsManager::HttpProxy:
            mProxyHost->setVisible(true);
            mProxyHostLabel->setVisible(true);
            mProxyPort->setVisible(true);
            mProxyPortLabel->setVisible(true);
            mProxyRequirePassword->setVisible(true);
            mProxyUsername->setVisible(true);
            mProxyUsernameLabel->setVisible(true);
            mProxyPassword->setVisible(true);
            mProxyPasswordLabel->setVisible(true);
            break;
        case SettingsManager::SocksProxy:
            mProxyHost->setVisible(true);
            mProxyHostLabel->setVisible(true);
            mProxyPort->setVisible(true);
            mProxyPortLabel->setVisible(true);
            mProxyRequirePassword->setVisible(false);
            mProxyUsername->setVisible(false);
            mProxyUsernameLabel->setVisible(false);
            mProxyPassword->setVisible(false);
            mProxyPasswordLabel->setVisible(false);
            break;
        case SettingsManager::NoProxy:
        case SettingsManager::SystemProxy:
        default:
            mProxyHost->setVisible(false);
            mProxyHostLabel->setVisible(false);
            mProxyPort->setVisible(false);
            mProxyPortLabel->setVisible(false);
            mProxyRequirePassword->setVisible(false);
            mProxyUsername->setVisible(false);
            mProxyUsernameLabel->setVisible(false);
            mProxyPassword->setVisible(false);
            mProxyPasswordLabel->setVisible(false);
            break;
    }

    if (proxy_type == SettingsManager::HttpProxy ||
        proxy_type == SettingsManager::SocksProxy) {
        QString prefix =
            proxy_type == SettingsManager::HttpProxy ? "http" : "socks";
        QSettings settings;
        QString key;
        settings.beginGroup(kSettingsGroupForSettingsDialog);
        if (mProxyHost->text().trimmed().isEmpty()) {
            key = prefix + "_proxy_host";
            if (settings.contains(key)) {
                mProxyHost->setText(settings.value(key).toString());
            }
        }
        if (mProxyPort->value() == 0) {
            key = prefix + "_proxy_port";
            if (settings.contains(key)) {
                mProxyPort->setValue(settings.value(key).toInt());
            }
        }
    }
}

// Called when the user clicked "OK" button of the settings dialog. Return
// true if the proxy settings has been changed by the user.
bool SettingsDialog::updateProxySettings()
{
    SettingsManager *mgr = gui->settingsManager();
    SettingsManager::SeafileProxy old_proxy = mgr->getProxy();

    SettingsManager::ProxyType proxy_type = static_cast<SettingsManager::ProxyType>(mProxyMethodComboBox->currentIndex());
    QString proxy_host = mProxyHost->text().trimmed();
    QString proxy_username = mProxyUsername->text().trimmed();
    QString proxy_password = mProxyPassword->text().trimmed();
    int proxy_port = mProxyPort->value();

    SettingsManager::SeafileProxy new_proxy(proxy_type);

    switch(proxy_type) {
        case SettingsManager::HttpProxy:
            new_proxy.host = proxy_host;
            new_proxy.port = proxy_port;
            if (mProxyRequirePassword->checkState() == Qt::Checked) {
                new_proxy.username = proxy_username;
                new_proxy.password = proxy_password;
                break;
            }
            break;
        case SettingsManager::SocksProxy:
            new_proxy.host = proxy_host;
            new_proxy.port = proxy_port;
            break;
        case SettingsManager::NoProxy:
        case SettingsManager::SystemProxy:
        default:
            break;
    }

    if (new_proxy != old_proxy) {
        mgr->setProxy(new_proxy);
        return true;
    }

    return false;
}

bool SettingsDialog::validateProxyInputs()
{
    SettingsManager::ProxyType proxy_type =
        static_cast<SettingsManager::ProxyType>(
            mProxyMethodComboBox->currentIndex());
    if (proxy_type == SettingsManager::NoProxy ||
        proxy_type == SettingsManager::SystemProxy) {
        return true;
    }

    QString proxy_host = mProxyHost->text().trimmed();
    if (proxy_host.isEmpty()) {
        gui->warningBox(tr("The proxy host address can't be empty"),
                               this);
        return false;
    }

    int proxy_port = mProxyPort->value();
    if (proxy_port == 0) {
        gui->warningBox(tr("The proxy port is incorrect"),
                               this);
        return false;
    }

    if (mProxyRequirePassword->checkState() == Qt::Checked) {
        QString proxy_username = mProxyUsername->text().trimmed();
        QString proxy_password = mProxyPassword->text().trimmed();
        if (proxy_username.isEmpty()) {
            gui->warningBox(tr("Proxy username can't be empty"), this);
            return false;
        } else if (proxy_password.isEmpty()) {
            gui->warningBox(tr("Proxy password can't be empty"), this);
            return false;
        }
    }

    QSettings settings;

    settings.beginGroup(kSettingsGroupForSettingsDialog);
    if (proxy_type == SettingsManager::HttpProxy) {
        settings.setValue("http_proxy_host", proxy_host);
        settings.setValue("http_proxy_port", proxy_port);
    } else if (proxy_type == SettingsManager::SocksProxy) {
        settings.setValue("socks_proxy_host", proxy_host);
        settings.setValue("socks_proxy_port", proxy_port);
    }
    settings.endGroup();

    return true;
}

void SettingsDialog::selectDirAction()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Please choose the cache folder"),
                                                    current_cache_dir_.toUtf8().data(),
                                                    QFileDialog::ShowDirsOnly
                                                    | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty())
        return;
    //setDirectoryText(dir);
    QString text = dir;
    if (text.endsWith("/")) {
        text.resize(text.size() - 1);
    }
    mShowCacheDir->setText(text);

}

void SettingsDialog::onOkBtnClicked()
{
    if (!validateProxyInputs()) {
        return;
    }
    updateSettings();
    accept();
}
