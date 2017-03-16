#include "advanced-sharedlink-dialog.h"

#include <QtGlobal>
#include <QtWidgets>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QGridLayout>
#include "utils/utils-mac.h"

AdvancedSharedLinkDialog::AdvancedSharedLinkDialog(const QString &text, QWidget *parent)
  : text_(text)
{
    setWindowTitle(tr("Share Link"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) |
                   Qt::WindowStaysOnTopHint);
    QVBoxLayout *layout = new QVBoxLayout;

    QLabel *label = new QLabel(tr("Share link:"));
    layout->addWidget(label);
    layout->setSpacing(5);
    layout->setContentsMargins(9, 9, 9, 9);

    editor_ = new QLineEdit;
    editor_->setText(text_);
    editor_->selectAll();
    editor_->setReadOnly(true);
    layout->addWidget(editor_);

    QGroupBox *pwdGroupBox = new QGroupBox("Add password protection");
    pwdGroupBox->setCheckable(true);
    pwdGroupBox->setChecked(true);
    QGridLayout *gridLayout = new QGridLayout();
    QLabel *pwdLabel = new QLabel(tr("password:"));
    QLineEdit *pwdEdit = new QLineEdit;
    pwdEdit->setEchoMode(QLineEdit::Password);
    QLabel *pwdLabel2 = new QLabel(tr("password again:"));
    QLineEdit *pwdEdit2 = new QLineEdit;
    pwdEdit2->setEchoMode(QLineEdit::Password);
    gridLayout->addWidget(pwdLabel, 0, 0);
    gridLayout->addWidget(pwdEdit, 0, 1);
    gridLayout->addWidget(pwdLabel2, 1, 0);
    gridLayout->addWidget(pwdEdit2, 1, 1);
    pwdGroupBox->setLayout(gridLayout);
    layout->addWidget(pwdGroupBox);

    QGroupBox *expiredDateGroupBox = new QGroupBox("Add auto expiration");
    expiredDateGroupBox->setCheckable(true);
    expiredDateGroupBox->setChecked(true);
    QHBoxLayout *expiredDateLayout = new QHBoxLayout(); 
    QLabel *expiredDateLabel = new QLabel(tr("Days:"));
    QSpinBox *expiredDateSpinBox = new QSpinBox();
    expiredDateSpinBox->setMinimum(1);
    expiredDateLayout->addWidget(expiredDateLabel);
    expiredDateLayout->addWidget(expiredDateSpinBox);
    expiredDateGroupBox->setLayout(expiredDateLayout); 
    layout->addWidget(expiredDateGroupBox);

    QHBoxLayout *hlayout = new QHBoxLayout;

    QWidget *spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    hlayout->addWidget(spacer);

    QWidget *spacer2 = new QWidget;
    spacer2->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    hlayout->addWidget(spacer2);

    QPushButton *copy_to = new QPushButton(tr("Copy to clipboard"));
    hlayout->addWidget(copy_to);
    connect(copy_to, SIGNAL(clicked()), this, SLOT(onCopyText()));

    QPushButton *ok = new QPushButton(tr("OK"));
    hlayout->addWidget(ok);
    connect(ok, SIGNAL(clicked()), this, SLOT(accept()));

    layout->addLayout(hlayout);

    setLayout(layout);

    setMinimumWidth(300);
    setMaximumWidth(400);
}

void AdvancedSharedLinkDialog::onCopyText()
{
// for mac, qt copys many minedatas beside public.utf8-plain-text
// e.g. public.vcard, which we don't want to use
#ifndef Q_OS_MAC
    QApplication::clipboard()->setText(editor_->text());
#else
    utils::mac::copyTextToPasteboard(editor_->text());
#endif
}

void AdvancedSharedLinkDialog::onDownloadStateChanged(int state)
{
    if (state == Qt::Checked)
        editor_->setText(text_ + "?dl=1");
    else
        editor_->setText(text_);
}
