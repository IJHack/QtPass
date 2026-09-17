// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_STORETREE_H_
#define SRC_STORETREE_H_

#include "storemodel.h"
#include <QFileInfo>
#include <QFileSystemModel>
#include <QModelIndex>
#include <QObject>
#include <QString>

class Pass;
class QTreeView;

/**
 * @class StoreTree
 * @brief The password store as shown in the tree: the QFileSystemModel, the
 *        StoreModel proxy on top of it and the QTreeView they drive, with
 *        the path questions the main window keeps asking.
 *
 * Owns the two models. setStore() is the one place that re-roots the file
 * system model, the proxy and the view together (this sequence used to be
 * spelled out five times); dirFor()/fileFor() are the two path mappings
 * that were duplicated across MainWindow and Util.
 */
class StoreTree : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Set the view up for the store: models, hidden columns, sorting,
   *        context-menu policy.
   * @param view The tree view to drive.
   * @param parent Owner.
   */
  explicit StoreTree(QTreeView *view, QObject *parent = nullptr);

  /**
   * @brief Point everything at @p path: file system root, proxy store and
   *        view root index. Also drops a search filter.
   * @param path Password store directory.
   */
  void setStore(const QString &path);

  /**
   * @brief Hand the proxy the backend it drops onto.
   * @param pass Active backend (not owned).
   */
  void setPass(Pass *pass);

  /**
   * @brief Filter the visible entries.
   * @param pattern Regular expression matched against entry names; an
   *                invalid or empty one shows everything.
   */
  void setFilter(const QRegularExpression &pattern);

  /**
   * @brief The folder an index stands for.
   * @param index View index; a file maps to its folder, an invalid index to
   *              the store root.
   * @param forPass true for a store-relative path ("" for the root),
   *                false for an absolute one.
   * @return Folder path ending in the native separator (see forPass).
   */
  [[nodiscard]] auto dirFor(const QModelIndex &index, bool forPass) const
      -> QString;

  /**
   * @brief dirFor() applied to the view's current index.
   * @param forPass As in dirFor().
   * @return Folder path.
   */
  [[nodiscard]] auto currentDir(bool forPass) const -> QString;

  /**
   * @brief The entry an index stands for.
   * @param index View index; folders and invalid indexes yield an empty
   *              string.
   * @param forPass true for the pass name (store-relative, no .gpg),
   *                false for the absolute .gpg path.
   * @return Entry name or path, empty when @p index is not a file.
   */
  [[nodiscard]] auto fileFor(const QModelIndex &index, bool forPass) const
      -> QString;

  /**
   * @brief fileFor() applied to the view's current index.
   * @param forPass As in fileFor().
   * @return Entry name or path.
   */
  [[nodiscard]] auto currentFile(bool forPass) const -> QString;

  /**
   * @brief File system information for a view index.
   * @param index View index.
   * @return QFileInfo of the underlying path.
   */
  [[nodiscard]] auto fileInfo(const QModelIndex &index) const -> QFileInfo;

  /**
   * @brief The view index for an absolute path, or invalid when the file
   *        system model does not (yet) know it.
   * @param path Absolute path inside the store.
   * @return Proxy index.
   */
  [[nodiscard]] auto indexFor(const QString &path) const -> QModelIndex;

  /**
   * @brief The proxy's root index for the current store.
   * @return Proxy index of the store root.
   */
  [[nodiscard]] auto rootIndex() -> QModelIndex;

  /**
   * @brief The view's current index.
   * @return Proxy index, possibly invalid.
   */
  [[nodiscard]] auto currentIndex() const -> QModelIndex;

  /// @return The store directory set with setStore().
  [[nodiscard]] auto store() const -> QString { return m_store; }
  /// @return The proxy the view shows.
  [[nodiscard]] auto proxy() -> StoreModel & { return m_proxy; }
  /// @return The file system model under the proxy.
  [[nodiscard]] auto fileSystem() -> QFileSystemModel & { return m_fs; }
  /// @return The tree view.
  [[nodiscard]] auto view() const -> QTreeView * { return m_view; }

private:
  QTreeView *m_view;
  QFileSystemModel m_fs;
  StoreModel m_proxy;
  QString m_store;
};

#endif // SRC_STORETREE_H_
