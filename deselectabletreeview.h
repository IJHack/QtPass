#ifndef DESELECTABLETREEVIEW_H
#define DESELECTABLETREEVIEW_H
#include "QDebug"
#include "QMouseEvent"
#include "QTreeView"
#include "mainwindow.h"

/**
 * @brief The DeselectableTreeView class
 * taken from http://stackoverflow.com/questions/2761284/
 * thanks to Yassir Ennazk
 */
class DeselectableTreeView : public QTreeView {
  Q_OBJECT

public:
  /**
   * @brief DeselectableTreeView standard constructor
   * @param parent
   */
  DeselectableTreeView(QWidget *parent) : QTreeView(parent) {}
  /**
   * @brief ~DeselectableTreeView standard destructor
   */
  virtual ~DeselectableTreeView() {}

signals:
  /**
   * @brief emptyClicked event
   */
  void emptyClicked();

private:
  /**
   * @brief mousePressEvent now deselects on second click
   * @param event
   */
  virtual void mousePressEvent(QMouseEvent *event) {
    QModelIndex item = indexAt(event->pos());
    bool selected = selectionModel()->isSelected(indexAt(event->pos()));
    QTreeView::mousePressEvent(event);
    if ((item.row() == -1 && item.column() == -1) || selected) {
      clearSelection();
      const QModelIndex index;
      selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select);
      emit emptyClicked();
      // QTreeView::mousePressEvent(event);
    }
  }
};

#endif // DESELECTABLETREEVIEW_H
