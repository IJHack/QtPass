// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_DESELECTABLETREEVIEW_H_
#define SRC_DESELECTABLETREEVIEW_H_

#include <QGuiApplication>
#include <QMouseEvent>
#include <QStyleHints>
#include <QTimer>
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
  explicit DeselectableTreeView(QWidget *parent) : QTreeView(parent) {
    deselectTimer.setSingleShot(true);
    connect(&deselectTimer, &QTimer::timeout, this,
            &DeselectableTreeView::deselect);
  }
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
  QTimer deselectTimer;

  /**
   * @brief mousePressEvent registers if the field was preselected
   * @param event
   */
  void mousePressEvent(QMouseEvent *event) override {
    // A new press belongs to a new click: whatever the previous release left
    // pending must not clear the selection this press is about to make.
    deselectTimer.stop();
    clickSelected =
        selectionModel()->isSelected(indexAt(event->position().toPoint()));
    QTreeView::mousePressEvent(event);
  }

  /**
   * @brief mouseReleaseEvent now deselects on click on empty space
   * @param event
   */
  void mouseReleaseEvent(QMouseEvent *event) override {
    const QModelIndex item = indexAt(event->position().toPoint());
    const bool pressWasOnSelection = clickSelected;
    clickSelected = false;

    // Handle the release immediately: waiting here would delay selection —
    // and therefore every decrypt — by the double-click interval.
    QTreeView::mouseReleaseEvent(event);

    const bool emptyOrSelected =
        !item.isValid() || selectionModel()->isSelected(item);
    if (!pressWasOnSelection || !emptyOrSelected)
      return;

    // Deselecting is the one decision that must wait: a double-click starts
    // with a release, and opening the editor must win over clearing the
    // selection. Ask the platform how long a double-click may take instead
    // of assuming 200 ms.
    doubleClickHappened = false;
    deselectTimer.start(
        QGuiApplication::styleHints()->mouseDoubleClickInterval());
  }

  void deselect() {
    if (doubleClickHappened)
      return;
    clearSelection();
    selectionModel()->setCurrentIndex(QModelIndex(),
                                      QItemSelectionModel::Select);
    emit emptyClicked();
  }

  /**
   * @brief mouseDoubleClickEvent
   * @param event
   */
  void mouseDoubleClickEvent(QMouseEvent *event) override {
    doubleClickHappened = true;
    deselectTimer.stop();
    QTreeView::mouseDoubleClickEvent(event);
  }
};

#endif // SRC_DESELECTABLETREEVIEW_H_
