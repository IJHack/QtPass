// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_GREPSEARCHCONTROLLER_H_
#define SRC_GREPSEARCHCONTROLLER_H_

/**
 * @class GrepSearchController
 * @brief State machine for MainWindow's content-search (grep) mode.
 *
 * Extracted from MainWindow, where the search state lived in three loose
 * booleans (mode / busy / cancelled) poked from six different slots. This
 * owns those flags behind intent-named transitions so the slots no longer
 * introspect raw booleans; each transition returns what UI side effect the
 * caller still has to perform (wait-cursor handling, discarding stale
 * results). It holds no widgets — MainWindow keeps those and reacts to the
 * returned outcomes.
 */
class GrepSearchController {
public:
  /**
   * @brief Outcome of a search-completion transition.
   */
  struct FinishOutcome {
    bool restoreCursor; ///< A wait cursor was active and must be restored.
    bool discard;       ///< The finished search was cancelled; ignore results.
  };

  /**
   * @brief Whether content-search mode is currently active.
   * @return true if in grep mode.
   */
  auto inGrepMode() const -> bool { return m_mode; }

  /**
   * @brief Enter content-search mode (search button toggled on).
   */
  void enterGrepMode() { m_mode = true; }

  /**
   * @brief Leave content-search mode (search button toggled off).
   *
   * If a search was in flight it is marked cancelled so its pending results
   * are discarded when they arrive.
   * @return true if a busy search was interrupted (restore the wait cursor).
   */
  auto leaveGrepMode() -> bool {
    m_mode = false;
    if (m_busy) {
      m_busy = false;
      m_cancelled = true;
      return true;
    }
    return false;
  }

  /**
   * @brief Drop the mode flag only, without touching busy/cancelled state.
   *
   * Used when the panel is cleared and the search UI is reset in place.
   */
  void clearGrepMode() { m_mode = false; }

  /**
   * @brief Begin a search for a non-empty query.
   * @return true if a wait cursor should now be shown (search newly busy).
   */
  auto beginSearch() -> bool {
    m_cancelled = false;
    if (!m_busy) {
      m_busy = true;
      return true;
    }
    return false;
  }

  /**
   * @brief Cancel the current search (e.g. query cleared).
   * @return true if a wait cursor should be restored (a search was busy).
   */
  auto cancelSearch() -> bool {
    m_cancelled = true;
    if (m_busy) {
      m_busy = false;
      return true;
    }
    return false;
  }

  /**
   * @brief Record that a search finished and report required side effects.
   * @return Whether to restore the wait cursor and whether to discard the
   * results (because the search was cancelled).
   */
  auto finishSearch() -> FinishOutcome {
    FinishOutcome outcome{m_busy, m_cancelled};
    m_busy = false;
    m_cancelled = false;
    return outcome;
  }

private:
  bool m_mode = false;      ///< Content-search mode active.
  bool m_busy = false;      ///< A grep subprocess is in flight.
  bool m_cancelled = false; ///< Pending results should be discarded.
};

#endif // SRC_GREPSEARCHCONTROLLER_H_
