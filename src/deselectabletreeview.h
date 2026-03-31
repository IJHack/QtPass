// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_DESELECTABLETREEVIEW_H_
#define SRC_DESELECTABLETREEVIEW_H_

#include <QCoreApplication>
#include <QMouseEvent>
#include <QTime>
#include <QTreeView>

/**
 * A QTreeView subclass that clears selection and emits a signal when the user
 * clicks an empty area or clicks a selected item in empty space.
 *
 * The view tracks press/release sequences to distinguish single- from
 * double-clicks and only clears selection for qualifying single-clicks.
 */

/**
 * Construct a DeselectableTreeView.
 * @param parent Parent widget forwarded to QTreeView.
 */

/**
 * Default destructor.
 */

/**
 * Emitted after the view clears its selection in response to a qualifying
 * click on empty space or on an already-selected item.
 */

/**
 * Record whether the item under the press position was selected.
 * @param event Mouse press event containing the press position.
 */

/**
 * Clear selection and emit emptyClicked() when a qualifying single-click
 * releases over empty space or over a selected item; otherwise, handle the
 * release normally.
 * @param event Mouse release event containing the release position.
 */

/**
 * Mark that a double-click occurred and handle the double-click normally.
 * @param event Mouse double-click event.
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
  ~DeselectableTreeView() override = default;

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
  void mousePressEvent(QMouseEvent *event) override {
    clickSelected = selectionModel()->isSelected(indexAt(event->pos()));
    QTreeView::mousePressEvent(event);
  }

  /**
   * @brief mouseReleaseEvent now deselects on click on empty space
   * @param event
   */
  void mouseReleaseEvent(QMouseEvent *event) override {
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
  void mouseDoubleClickEvent(QMouseEvent *event) override {
    doubleClickHappened = true;
    QTreeView::mouseDoubleClickEvent(event);
  }
};

#endif // SRC_DESELECTABLETREEVIEW_H_
