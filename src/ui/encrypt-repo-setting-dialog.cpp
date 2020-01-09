#include "encrypt-repo-setting-dialog.h"

#include <QIcon>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLineEdit>
#include <QtGlobal>


EncryptRepoSetting::EncryptRepoSetting(const bool is_set_password, QWidget *parent)
{
    setWindowTitle(tr("Show Encrypted Repo"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags((windowFlags() & ~Qt::WindowContextHelpButtonHint) |
                   Qt::WindowStaysOnTopHint);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setSpacing(5);
    layout->setContentsMargins(9, 9, 9, 9);

    if (is_set_password) {
        repo_password_line_edit_ = new QLineEdit;
        repo_password_line_edit_->setObjectName("repo_passwd_line_edit");
        repo_password_line_edit_->setEchoMode(QLineEdit::Password);
        repo_password_line_edit_->clear();
        layout->addWidget(repo_password_line_edit_);
    } else {
        QLabel * label = new QLabel;
        label->setObjectName("tiplabel");
        label->setText(tr("To unsync the repository, please click \"OK\" button"));
        label->setAlignment(Qt::AlignCenter);

        layout ->addWidget(label);
    }

    QHBoxLayout *hlayout = new QHBoxLayout;

    QPushButton *ok = new QPushButton(tr("OK"));
    hlayout->addWidget(ok);
    connect(ok, SIGNAL(clicked()), this, SLOT(accept()));
    if(is_set_password) {
        connect(ok, SIGNAL(clicked()), this, SLOT(slotSetPassword()));
    }
    ok->setFocus();

    QPushButton *cancel = new QPushButton(tr("Cancel"));
    hlayout->addWidget(cancel);
    connect(cancel, SIGNAL(clicked()), this, SLOT(reject()));

    layout->addLayout(hlayout);

    setLayout(layout);
    setMinimumWidth(200);

}

QString EncryptRepoSetting::getRepoPassword() {
    return password_;
}

void EncryptRepoSetting::slotSetPassword() {
   password_ = repo_password_line_edit_->text();
}

