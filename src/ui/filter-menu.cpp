#include "filter-menu.h"


namespace
{
const QString kFileTypeTextFile = "Text";
const QString kFileTypeDocument = "Document";
const QString kFileTypeImage = "Image";
const QString kFileTypeVideo = "Video";
const QString kFileTypeAudio = "Audio";
const QString kFileTypePdf = "PDF";
const QString kFileTypeMarkdown = "Markdown";

} // namespace

FilterMenu::FilterMenu(QWidget *parent)
    : QWidget(parent),
      filter_list_(QStringList())
{
    setupUi(this);
    setStyleSheet("QWidget#mFilter {"
                      "border-bottom : 1px solid #d0d0d0;}");
    connect(mTextFile, SIGNAL(clicked(bool)),
            this, SLOT(onBoxChanged()));
    connect(mDocument, SIGNAL(clicked(bool)),
            this, SLOT(onBoxChanged()));
    connect(mImage, SIGNAL(clicked(bool)),
            this, SLOT(onBoxChanged()));
    connect(mVideo, SIGNAL(clicked(bool)),
            this, SLOT(onBoxChanged()));
    connect(mAudio, SIGNAL(clicked(bool)),
            this, SLOT(onBoxChanged()));
    connect(mMarkdown, SIGNAL(clicked(bool)),
            this, SLOT(onBoxChanged()));
}

void FilterMenu::onBoxChanged()
{
    if (!filterList().isEmpty()) {
        emit filterChanged();
    }
}

void FilterMenu::clearCheckBox()
{
    mTextFile->setChecked(false);
    mDocument->setChecked(false);
    mImage->setChecked(false);
    mVideo->setChecked(false);
    mAudio->setChecked(false);
    mMarkdown->setChecked(false);
}

QStringList FilterMenu::filterList() {
    QStringList types;
    if (mTextFile->isChecked()) {
        types.append(kFileTypeTextFile);
    }
    if (mDocument->isChecked()) {
        types.append(kFileTypeDocument);
        types.append(kFileTypePdf);
    }
    if (mImage->isChecked()) {
        types.append(kFileTypeImage);
    }
    if (mVideo->isChecked()) {
        types.append(kFileTypeVideo);
    }
    if (mAudio->isChecked()) {
        types.append(kFileTypeAudio);
    }
    if (mMarkdown->isChecked()) {
        types.append(kFileTypeMarkdown);
    }
    return types;
}
