#include "seafilelink-dialog.h"

#include <QtGlobal>
#include <QtWidgets>
// #include "QtAwesome.h"
#include "account.h"
#include "utils/utils.h"
#include "utils/utils-mac.h"

namespace {

const char *kSeafileProtocolScheme = "seafile";
const char *kSeafileProtocolHostOpenFile = "openfile";

} // namespace

SeafileLinkDialog::SeafileLinkDialog(const QString& smart_link, QWidget *parent)
    :web_link_(smart_link)
{
    setWindowTitle(tr("%1 Internal Link").arg(getBrand()));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

//    {
//        QString fixed_path = path.startsWith("/") ? path : "/" + path;
//        if (fixed_path.endsWith("/"))
//            web_link_ = account.getAbsoluteUrl(
//                                "/#common/lib/" + repo_id +
//                                (fixed_path == "/" ? "/" : fixed_path.left(fixed_path.size() - 1)))
//                            .toEncoded();
//        else
//            web_link_ =
//                account.getAbsoluteUrl("/lib/" + repo_id + "/file" + fixed_path).toEncoded();
//    }

//    {
//        QUrl url;
//        url.setScheme(kSeafileProtocolScheme);
//        url.setHost(kSeafileProtocolHostOpenFile);

//        QUrlQuery url_query;
//        url_query.addQueryItem("repo_id",  repo_id);
//        url_query.addQueryItem("path",  path);
//        url.setQuery(url_query);

//        protocol_link_ = url.toEncoded();
//    }

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setSpacing(5);
    layout->setContentsMargins(9, 9, 9, 9);

    QString copy_to_str = tr("Copy to clipboard");
    QIcon copy_to_icon(":/images/copy.png");

    //
    // create web link related
    //
    QLabel *web_label = new QLabel(tr("%1 Internal Link:").arg(getBrand()));
    layout->addWidget(web_label);
    QHBoxLayout *web_layout = new QHBoxLayout;

    web_editor_ = new QLineEdit;
    web_editor_->setText(web_link_);
    web_editor_->selectAll();
    web_editor_->setReadOnly(true);
    web_editor_->setCursorPosition(0);

    web_layout->addWidget(web_editor_);

    QPushButton *web_copy_to = new QPushButton;
    web_copy_to->setIcon(copy_to_icon);
    web_copy_to->setIconSize(QSize(16, 16));
    web_copy_to->setToolTip(copy_to_str);
    web_layout->addWidget(web_copy_to);
    connect(web_copy_to, SIGNAL(clicked()), this, SLOT(onCopyWebText()));
    layout->addLayout(web_layout);

    //
    // create seafile-protocol link related
    //
//    QLabel *protocol_label = new QLabel(tr("%1 Protocol Link:").arg(getBrand()));
//    layout->addWidget(protocol_label);
//    QHBoxLayout *protocol_layout = new QHBoxLayout;

//    protocol_editor_ = new QLineEdit;
//    protocol_editor_->setText(protocol_link_);
//    protocol_editor_->selectAll();
//    protocol_editor_->setReadOnly(true);
//    protocol_editor_->setCursorPosition(0);

//    protocol_layout->addWidget(protocol_editor_);

//    QPushButton *protocol_copy_to = new QPushButton;
//    protocol_copy_to->setIcon(copy_to_icon);
//    protocol_copy_to->setIconSize(QSize(16, 16));
//    protocol_copy_to->setToolTip(copy_to_str);
//    protocol_layout->addWidget(protocol_copy_to);
//    connect(protocol_copy_to, SIGNAL(clicked()), this, SLOT(onCopyProtocolText()));
//    layout->addLayout(protocol_layout);

    QHBoxLayout *hlayout = new QHBoxLayout;

    QWidget *spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    hlayout->addWidget(spacer);

    QWidget *spacer2 = new QWidget;
    spacer2->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    hlayout->addWidget(spacer2);

    QWidget *spacer3 = new QWidget;
    spacer3->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    hlayout->addWidget(spacer3);

    QPushButton *ok = new QPushButton(tr("OK"));
    hlayout->addWidget(ok);
    connect(ok, SIGNAL(clicked()), this, SLOT(accept()));
    ok->setFocus();

    layout->addLayout(hlayout);

    setLayout(layout);

    setMinimumWidth(450);
}

void SeafileLinkDialog::onCopyWebText()
{
// for mac, qt copys many minedatas beside public.utf8-plain-text
// e.g. public.vcard, which we don't want to use
#ifndef Q_OS_MAC
    QApplication::clipboard()->setText(web_link_);
#else
    utils::mac::copyTextToPasteboard(web_link_);
#endif
}

void SeafileLinkDialog::onCopyProtocolText()
{
// for mac, qt copys many minedatas beside public.utf8-plain-text
// e.g. public.vcard, which we don't want to use
#ifndef Q_OS_MAC
    QApplication::clipboard()->setText(protocol_link_);
#else
    utils::mac::copyTextToPasteboard(protocol_link_);
#endif
}
