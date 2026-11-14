// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief DeselectableTreeView: a QTreeView that clears its selection when
 *        empty space or an already-selected row is clicked.
 */
#ifndef SRC_DESELECTABLETREEVIEW_H_
#define SRC_DESELECTABLETREEVIEW_H_

#include <QGuiApplication>
#include <QMouseEvent>
#include <QStyleHints>
#include <QTimer>
#include <QTreeView>

/**
 * @class DeselectableTreeView
 * @ingroup ui
 * @brief A QTreeView that clears its selection, and emits emptyClicked(),
 *        when the user clicks empty space or an already-selected item.
 *
 * The press records whether the row under the pointer was selected, the
 * release handles selection immediately and arms a single-shot timer for the
 * platform's double-click interval, and only when no double-click arrives in
 * that time does deselect() clear the selection. Opening an entry by
 * double-click therefore always wins over deselecting it.
 */
class DeselectableTreeView : public QTreeView {
  Q_OBJECT

public:
  /**
   * @brief Construct the view and arm the single-shot deselect timer.
   * @param parent Parent widget forwarded to QTreeView.
   */
  explicit DeselectableTreeView(QWidget *parent) : QTreeView(parent) {
    deselectTimer.setSingleShot(true);
    connect(&deselectTimer, &QTimer::timeout, this,
            &DeselectableTreeView::deselect);
  }
  /**
   * @brief Default destructor.
   */
  ~DeselectableTreeView() override = default;

signals:
  /**
   * @brief Emitted after deselect() has cleared the selection because of a
   *        single click on empty space or on a selected row; MainWindow
   *        clears the password panel in response.
   */
  void emptyClicked();

private:
  /**
   * @brief Set by mouseDoubleClickEvent() so a deselect() that fires after a
   *        double-click's first release does nothing.
   */
  bool doubleClickHappened = false;
  /**
   * @brief Whether the row under the last press was already selected; read
   *        and reset by the matching release.
   */
  bool clickSelected = false;
  /**
   * @brief Single-shot timer, started by a qualifying release for one
   *        double-click interval; its timeout runs deselect().
   */
  QTimer deselectTimer;

  /**
   * @brief Remember whether the pressed row was already selected and cancel
   *        any pending deselect from the previous click.
   * @param event The mouse press; forwarded to QTreeView.
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
   * @brief Let QTreeView handle the release at once, then, if the press was
   *        on a selected row and the release is over empty space or a still
   *        selected row, start deselectTimer.
   * @param event The mouse release; forwarded to QTreeView first.
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

  /**
   * @brief Timer slot: clear the selection and current index and emit
   *        emptyClicked(), unless a double-click happened in the meantime.
   */
  void deselect() {
    if (doubleClickHappened)
      return;
    clearSelection();
    selectionModel()->setCurrentIndex(QModelIndex(),
                                      QItemSelectionModel::Select);
    emit emptyClicked();
  }

  /**
   * @brief Cancel the pending deselect so the double-click opens the entry
   *        instead of clearing the selection.
   * @param event The double-click; forwarded to QTreeView.
   */
  void mouseDoubleClickEvent(QMouseEvent *event) override {
    doubleClickHappened = true;
    deselectTimer.stop();
    QTreeView::mouseDoubleClickEvent(event);
  }
};

#endif // SRC_DESELECTABLETREEVIEW_H_
