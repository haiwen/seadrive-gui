#ifndef FILTERMENU_H
#define FILTERMENU_H

#include <QWidget>
#include "ui_filter-menu.h"

class FilterMenu : public QWidget,
                   public Ui::FilterMenu
{
    Q_OBJECT
public:
    FilterMenu(QWidget *parent = 0);
    QStringList filterList();
    void clearCheckBox();
signals:
    void filterChanged();
private slots:

    void onBoxChanged();

private:
    void boxChanged();
    Q_DISABLE_COPY(FilterMenu)
    QStringList last_filters_;
};

#endif // FILTERMENU_H
