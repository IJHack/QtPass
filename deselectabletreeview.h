#ifndef DESELECTABLETREEVIEW_H
#define DESELECTABLETREEVIEW_H
#include "QDebug"
#include "QMouseEvent"
#include "QTreeView"
#include "mainwindow.h"
#include <QTime>

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
  bool DoubleClickHappened;

  /**
   * @brief mouseReleaseEvent now deselects on click on empty space
   * @param event
   */
  void mouseReleaseEvent(QMouseEvent *event) {
    DoubleClickHappened = false;
    // The timer is to distinguish between single and double click
    QTime dieTime= QTime::currentTime().addMSecs(200);
    while (QTime::currentTime() < dieTime)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    if (!DoubleClickHappened){
      QModelIndex item = indexAt(event->pos());
      bool selected = selectionModel()->isSelected(indexAt(event->pos()));
      QTreeView::mouseReleaseEvent(event);
      if ((item.row() == -1 && item.column() == -1) || selected) {
        clearSelection();
        const QModelIndex index;
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select);
        emit emptyClicked();
      }
    }
  }


  /**
   * @brief mouseDoubleClickEvent
   * @param event
   */
  void mouseDoubleClickEvent(QMouseEvent *event){
    DoubleClickHappened = true;
    QTreeView::mouseDoubleClickEvent(event);
  }

};

#endif // DESELECTABLETREEVIEW_H
