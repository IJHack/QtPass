#ifndef DESELECTABLETREEVIEW_H
#define DESELECTABLETREEVIEW_H

#include <QCoreApplication>
#include <QMouseEvent>
#include <QTreeView>
#include <QTime>

/**
 * @brief The DeselectableTreeView class
 * loosly based on http://stackoverflow.com/questions/2761284/
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
  bool doubleClickHappened = false;
  bool clickSelected = false;

  /**
   * @brief mousePressEvent registers if the field was pre-selected
   * @param event
   */
  virtual void mousePressEvent(QMouseEvent *event) {
    clickSelected = selectionModel()->isSelected(indexAt(event->pos()));
    QTreeView::mousePressEvent(event);
  }

  /**
   * @brief mouseReleaseEvent now deselects on click on empty space
   * @param event
   */
  void mouseReleaseEvent(QMouseEvent *event) {
    doubleClickHappened = false;
    // The timer is to distinguish between single and double click
    QTime dieTime = QTime::currentTime().addMSecs(200);
    while (QTime::currentTime() < dieTime)
      QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
    // could this be done nicer?
    if (!doubleClickHappened && clickSelected) {
      QModelIndex item = indexAt(event->pos());
      bool selected = selectionModel()->isSelected(indexAt(event->pos()));
      if ((item.row() == -1 && item.column() == -1) || selected) {
        clearSelection();
        const QModelIndex index;
        selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select);
        emit emptyClicked();
      } else {
        QTreeView::mouseReleaseEvent(event);
      }
    } else {
      QTreeView::mouseReleaseEvent(event);
    }
    clickSelected = false;
  }

  /**
   * @brief mouseDoubleClickEvent
   * @param event
   */
  void mouseDoubleClickEvent(QMouseEvent *event) {
    doubleClickHappened = true;
    QTreeView::mouseDoubleClickEvent(event);
  }
};

#endif // DESELECTABLETREEVIEW_H
