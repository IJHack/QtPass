#ifndef DESELECTABLETREEVIEW_H
#define DESELECTABLETREEVIEW_H
/* taken from http://stackoverflow.com/questions/2761284/ thanks to Yassir Ennazk */
#include "QTreeView"
#include "QMouseEvent"
#include "QDebug"
#include "mainwindow.h"

class DeselectableTreeView : public QTreeView
{
     Q_OBJECT

public:
    DeselectableTreeView(QWidget *parent) : QTreeView(parent) {}
    virtual ~DeselectableTreeView() {}

signals:
    void emptyClicked();

private:
    virtual void mousePressEvent(QMouseEvent *event)
    {
        QModelIndex item = indexAt(event->pos());
        bool selected = selectionModel()->isSelected(indexAt(event->pos()));
        QTreeView::mousePressEvent(event);
        if ((item.row() == -1 && item.column() == -1) || selected)
        {
            clearSelection();
            const QModelIndex index;
            selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select);
            emit emptyClicked();
            //QTreeView::mousePressEvent(event);

        }
    }
};

#endif // DESELECTABLETREEVIEW_H
