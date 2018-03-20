#include "filter-menu.h"


namespace
{
const QString kTextFile = "Text";
const QString kDocument = "Document,PDF";
const QString kImage = "Image";
const QString kVideo = "Video";
const QString kAudio = "Audio";
const QString kPdf = "PDF";
const QString kMarkdown = "Markdown";

} // namespace

FilterMenu::FilterMenu(QWidget *parent)
    : QWidget(parent),
      filter_list_(QStringList())
{
    setupUi(this);
    setStyleSheet("QWidget#mFilter {"
                      "border-bottom : 1px solid #d0d0d0;}");
    connect(mTextFile, SIGNAL(clicked(bool)),
            this, SLOT(onTextFile(bool)));
    connect(mDocument, SIGNAL(clicked(bool)),
            this, SLOT(onDocument(bool)));
    connect(mImage, SIGNAL(clicked(bool)),
            this, SLOT(onImage(bool)));
    connect(mVideo, SIGNAL(clicked(bool)),
            this, SLOT(onVideo(bool)));
    connect(mAudio, SIGNAL(clicked(bool)),
            this, SLOT(onAudio(bool)));
    connect(mMarkdown, SIGNAL(clicked(bool)),
            this, SLOT(onMarkdown(bool)));
}

void FilterMenu::boxChanged(bool checked, const QString& text )
{
    if (checked) {
        filter_list_.append(text);
    } else {
        QString filter_list_member = filter_list_.join(",");
        filter_list_member.remove(text);
        filter_list_ = filter_list_member.split(",", QString::SkipEmptyParts);
    }
    sendFilterSignal();
}

void FilterMenu::sendFilterSignal()
{
    if (filter_list_.isEmpty()) {
        return;
    } else {
        emit filterChanged();
    }
}

void FilterMenu::onTextFile(bool checked)
{
    boxChanged(checked, kTextFile);
}

void FilterMenu::onDocument(bool checked)
{
    boxChanged(checked, kDocument);
    boxChanged(checked, kPdf);
}

void FilterMenu::onImage(bool checked)
{
    boxChanged(checked, kImage);
}

void FilterMenu::onVideo(bool checked)
{
    boxChanged(checked, kVideo);
}

void FilterMenu::onAudio(bool checked)
{
    boxChanged(checked, kAudio);
}

void FilterMenu::onMarkdown(bool checked)
{
    boxChanged(checked, kMarkdown);
}
