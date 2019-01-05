#include "sharedlink-dialog.h"

#include <QtGlobal>
#include <QtWidgets>
#include "utils/utils-mac.h"

#include "seadrive-gui.h"
#include "account-mgr.h"
#include <QComboBox>

#include <QtWidgets/QComboBox>

SharedLinkDialog::SharedLinkDialog(const ShareLinkInfo &linkInfo, QWidget *parent)
  : text_(linkInfo.officShareLink),
    linkInfo_(linkInfo)
{
    onAccountChanged();
    if (linkInfo.is_upload) {
        setWindowTitle(tr("Upload link"));
    }else{
        setWindowTitle(tr("DownLoad link"));
    }
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowIcon(QIcon(":/images/seafile.png"));
    QVBoxLayout *layout = new QVBoxLayout;

    QHBoxLayout *titleHlayout = new QHBoxLayout;

    QLabel *label = new QLabel;
    if (linkInfo.is_upload) {
        label->setText(tr("Upload link:"));
    }else{
        label->setText(tr("DownLoad link:"));

    }
    titleHlayout->addWidget(label);

    QWidget *spacer0 = new QWidget;
    spacer0->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    titleHlayout->addWidget(spacer0);
    QFont font;
    font.setPointSize(10);
    linkValidity_label_ = new QLabel;
    linkValidity_label_->setText(tr("Link validity:"));
    linkValidity_label_->setFont(font);

    linkValidity_label_->setMaximumWidth(70);
    titleHlayout->addWidget(linkValidity_label_);

    commboBox_ = new QComboBox;
    commboBox_->insertItem(0, tr("1day validity"));
    commboBox_->insertItem(1, tr("7day validity"));
    commboBox_->insertItem(2, tr("14day validity"));
    commboBox_->insertItem(3, tr("permanent"));
    commboBox_->setCurrentIndex(sharedLinkType(linkInfo.day));

    connect(commboBox_, SIGNAL(currentIndexChanged(int)),
            this, SLOT(oncurrentShareIndexChanged()));

    titleHlayout->addWidget(commboBox_);

    layout->addLayout(titleHlayout);
    QLabel *tipTitle = new QLabel;
    tipTitle->setAlignment(Qt::AlignLeft);
    tipTitle->setText(tr("Tip: changing your password or expiration date will result in a new link and the old link will fail"));
    layout->addWidget(tipTitle);

    editor_ = new QLineEdit;
    editor_->setText(text_);
    editor_->selectAll();
    editor_->setReadOnly(true);
    layout->addWidget(editor_);

    QHBoxLayout *hlayout = new QHBoxLayout;
    is_download_checked_ = new QCheckBox(tr("Set the password"));
    is_download_checked_->setChecked(linkInfo.password.length()>0);
    connect(is_download_checked_, SIGNAL(stateChanged(int)),
            this, SLOT(onDownloadStateChanged(int)));
    hlayout->addWidget(is_download_checked_);

    passwordEditor_ = new QLineEdit;
    passwordEditor_->setText(linkInfo.password);
    passwordEditor_->selectAll();
    passwordEditor_->setAlignment(Qt::AlignLeft);
    passwordEditor_->setMaximumWidth(85);
    passwordEditor_->setReadOnly(true);
    hlayout->addWidget(passwordEditor_);
    passwordEditor_->setHidden(linkInfo.password.length() > 0 ? false : true);



    QWidget *spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    hlayout->addWidget(spacer);

    copySuccess_label_ = new QLabel;
    copySuccess_label_->setAlignment(Qt::AlignCenter);
    hlayout->addWidget(copySuccess_label_);

    QWidget *spacer2 = new QWidget;
    spacer2->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    hlayout->addWidget(spacer2);

    copy_to_ = new QPushButton(tr("Copy to clipboard"));
    if (linkInfo.password.length() > 0) {
        copy_to_->setText(tr("Copy link password"));
    }else{
        copy_to_->setText(tr("Copy link"));
    }
    hlayout->addWidget(copy_to_);
    connect(copy_to_, SIGNAL(clicked()), this, SLOT(onCopyText()));

    QPushButton *ok = new QPushButton(tr("Close"));
    hlayout->addWidget(ok);
    connect(ok, SIGNAL(clicked()), this, SLOT(accept()));

    layout->addLayout(hlayout);
    QHBoxLayout *timeLayout = new QHBoxLayout;

    linkTime_label_ = new QLabel;
    linkTime_label_->setObjectName("linkTimeLabel");
    linkTime_label_->setAlignment(Qt::AlignLeft);
    linkTime_label_->setMaximumHeight(15);
    timeLayout->addWidget(linkTime_label_);
    if (linkInfo.day == 0) {
        linkTime_label_->setHidden(true);
    }else{
        if (linkInfo.expireTime) {
            linkTime_label_->setText(shareLinkTime(linkInfo.expireTime));
        }
        linkTime_label_->setHidden(false);
    }
    layout->addLayout(timeLayout);

    setLayout(layout);

    setMinimumWidth(500);
    setMaximumWidth(700);
    setMaximumHeight(200);

}

QString SharedLinkDialog::shareLinkTime(qint64 expireTime)
{
    QString time = QDateTime::fromMSecsSinceEpoch(expireTime).toString("yyyy-MM-dd hh:mm:ss");
    QString timeStr;
    timeStr = tr("Link valid until:") + time;
    return timeStr;
}

int SharedLinkDialog::sharedLinkType(int type)
{
    if (type == 1) {
        return 0;
    }else if (type == 7){
        return 1;
    }else if (type == 14){
        return 2;
    }else{
        return 3;
    }
}

void SharedLinkDialog::onCopyText()
{
// for mac, qt copys many minedatas beside public.utf8-plain-text
// e.g. public.vcard, which we don't want to use
    QString shareLink;
    if (passwordEditor_->text().length() > 0 && linkInfo_.password.length()) {
        shareLink = tr("links:") + editor_->text() + "\n" + tr("password:") +  passwordEditor_->text();
    }else{
        shareLink = tr("links:") + editor_->text();

    }
#ifndef Q_OS_MAC
    QApplication::clipboard()->setText(shareLink);
#else
    utils::mac::copyTextToPasteboard(shareLink);
#endif
    copySuccess_label_->setText(tr("copy link success"));
}

void SharedLinkDialog::onDownloadStateChanged(int state)
{
    bool creatPassword;
    if (state == Qt::Checked)
    {
        creatPassword = true;
    }else{
        creatPassword = false;
    }
    linkInfo_.creatPassword = creatPassword;
    is_download_checked_->setEnabled(false);
    commboBox_->setEnabled(false);
    copy_to_->setEnabled(false);
    copySuccess_label_->setText(tr(""));
    delectShareLink(linkInfo_);
}

void SharedLinkDialog::oncurrentShareIndexChanged()
{
    int index = commboBox_->currentIndex();
    int day = 7;
    if (index == 0) {
        day = 1;
    }else if (index == 1){
        day = 7;
    }else if (index == 2){
        day = 14;
    }else{
        day = 0;
    }
    linkInfo_.day = day;
    is_download_checked_->setEnabled(false);
    commboBox_->setEnabled(false);
    copy_to_->setEnabled(false);
    copySuccess_label_->setText(tr(""));
    delectShareLink(linkInfo_);

}

void SharedLinkDialog::delectShareLink(const ShareLinkInfo& linkInfo)
{
    UNShareLinkFileRequest *req = new UNShareLinkFileRequest(account_,linkInfo);
    connect(req, SIGNAL(success(const ShareLinkInfo&)),
            SLOT(unShareFileDirentSuccess(const ShareLinkInfo&)));

    connect(req, SIGNAL(failed(const ApiError&)),
            SLOT(unShareFileLinkDirentFailed(const ApiError&)));
    req->send();

}


void SharedLinkDialog::unShareFileDirentSuccess(const ShareLinkInfo& link)
{

    ReShareLinkFileRequest *req = new ReShareLinkFileRequest(account_,
                                                             link.repo_id,
                                                             link.path,
                                                             link.creatPassword,
                                                             link.is_dir,
                                                             link.is_upload,
                                                             link.day);
    connect(req, SIGNAL(success(const ShareLinkInfo&, const QString&)),
            SLOT(onReShareFileLinkDirentShareSuccess(const ShareLinkInfo&)));

    connect(req, SIGNAL(failed(const ApiError&)),
            SLOT(onReShareFileLinkDirentFailed(const ApiError&)));
    req->send();
}



void SharedLinkDialog::onReShareFileLinkDirentShareSuccess(const ShareLinkInfo &link)
{
    if (link.resultCode && link.officShareLink.length() > 0) {
        linkInfo_.officShareLink = link.officShareLink;
        linkInfo_.shareLinkId = link.shareLinkId;
        linkInfo_.password = link.password;
        linkInfo_.creatPassword = link.creatPassword;
        if (link.password.length() > 0) {
            passwordEditor_->show();

            passwordEditor_->setText(link.password);
            copy_to_->setText(tr("Copy link password"));

        }else{
            passwordEditor_->hide();
            passwordEditor_->setText(tr(""));
            copy_to_->setText(tr("Copy link"));

        }
        editor_->setText(link.officShareLink);

    }else{
        gui->warningBox(QString("%1").arg(link.resultMsg),this);
    }
    if (link.day == 0) {
        linkTime_label_->setHidden(true);
    }else{
        if (link.expireTime) {
            linkTime_label_->setHidden(false);
            linkTime_label_->setText(shareLinkTime(link.expireTime));
        }
    }
    is_download_checked_->setEnabled(true);
    commboBox_->setEnabled(true);
    copy_to_->setEnabled(true);
}

void SharedLinkDialog::onReShareFileLinkDirentFailed(const ApiError &error)
{
    is_download_checked_->setEnabled(true);
    commboBox_->setEnabled(true);
    copy_to_->setEnabled(true);
    gui->warningBox(tr("Share failed"), this);
}

void SharedLinkDialog::onAccountChanged()
{
    account_ = gui->accountManager()->currentAccount();
}
