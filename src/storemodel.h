// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_STOREMODEL_H_
#define SRC_STOREMODEL_H_

#include <QFileInfo>
#include <QSortFilterProxyModel>

/**
 * @class StoreModel
 * @brief QSortFilterProxyModel for filtering and displaying password store.
 *
 * StoreModel wraps QFileSystemModel to provide password store-specific
 * filtering and display functionality. It handles:
 * - Hiding .gpg-id files from display
 * - Removing .gpg extension from file names
 * - Supporting drag and drop operations
 * - Custom sorting (directories before files)
 */
class QFileSystemModel;

/**
 * @struct dragAndDropInfoPasswordStore
 * @brief Holds information for drag and drop operations in the password store.
 */
struct dragAndDropInfoPasswordStore {
  /**
   * @brief Type of the dragged item.
   */
  enum class ItemKind {
    Unknown,   /**< Item type has not been determined yet */
    Directory, /**< Dragged item is a directory */
    File       /**< Dragged item is a file */
  };

  ItemKind kind{ItemKind::Unknown}; /**< Type of dragged item */
  QString path;                     /**< Full path to the dragged item */
};

class QDataStream;

/**
 * @brief Serialize a dragAndDropInfoPasswordStore for the QtPass MIME type.
 * @param out Stream to write to.
 * @param info Drag/drop info to serialize.
 * @return The same stream, for chaining.
 */
auto operator<<(QDataStream &out, const dragAndDropInfoPasswordStore &info)
    -> QDataStream &;

/**
 * @brief Deserialize a dragAndDropInfoPasswordStore from the QtPass MIME type.
 *
 * Unrecognised kind bytes resolve to ItemKind::Unknown rather than being
 * rejected, so a future protocol bump remains decode-safe.
 *
 * @param in Stream to read from.
 * @param info Drag/drop info to populate.
 * @return The same stream, for chaining.
 */
auto operator>>(QDataStream &in, dragAndDropInfoPasswordStore &info)
    -> QDataStream &;

class StoreModel : public QSortFilterProxyModel {
  Q_OBJECT

private:
  QFileSystemModel *fs;
  QString store;

  auto parseDropData(const QMimeData *data,
                     dragAndDropInfoPasswordStore *outInfo) -> bool;
  auto executeDropAction(const dragAndDropInfoPasswordStore &info,
                         Qt::DropAction action, const QModelIndex &parent)
      -> bool;
  auto handleDirDrop(const QString &cleanedSrc, const QFileInfo &destFileinfo,
                     const QFileInfo &srcFileInfo, Qt::DropAction action)
      -> bool;
  auto handleFileDrop(const QString &cleanedSrc, const QString &cleanedDest,
                      const QFileInfo &destFileinfo, Qt::DropAction action)
      -> bool;
  auto handleFileToDirDrop(const QString &cleanedSrc,
                           const QString &cleanedDest, Qt::DropAction action)
      -> bool;
  auto handleFileToFileDrop(const QString &cleanedSrc,
                            const QString &cleanedDest, Qt::DropAction action)
      -> bool;

public:
  /**
   * @brief Construct a StoreModel.
   */
  StoreModel();

  /**
   * @brief Filter whether a row should be displayed.
   * @param source_row Row index in source model.
   * @param source_parent Parent index in source model.
   * @return true if row should be displayed.
   */
  [[nodiscard]] auto filterAcceptsRow(int source_row,
                                      const QModelIndex &source_parent) const
      -> bool override;

  /**
   * @brief Check if a specific index should be shown.
   * @param index Model index to check.
   * @return true if index should be visible.
   */
  [[nodiscard]] auto showThis(const QModelIndex &index) const -> bool;

  /**
   * @brief Initialize model with source model and store path.
   * @param sourceModel Filesystem model to wrap.
   * @param passStore Root path of password store.
   */
  void setModelAndStore(QFileSystemModel *sourceModel,
                        const QString &passStore);

  /**
   * @brief Update the store path used for filtering without changing the source
   * model.
   * @param passStore New root path of password store.
   */
  void setStore(const QString &passStore);

  /**
   * @brief Get display data for index.
   * @param index Model index.
   * @param role Data role (e.g., Qt::DisplayRole).
   * @return Display data for the role.
   */
  [[nodiscard]] auto data(const QModelIndex &index, int role) const
      -> QVariant override;

  /**
   * @brief Compare two indices for sorting.
   * @param source_left Left index to compare.
   * @param source_right Right index to compare.
   * @return true if left < right.
   */
  [[nodiscard]] auto lessThan(const QModelIndex &source_left,
                              const QModelIndex &source_right) const
      -> bool override;

  /**
   * @brief Get the password store root path.
   * @return Store path.
   */
  [[nodiscard]] auto getStore() const -> QString { return store; }

  // QAbstractItemModel interface
public:
  /**
   * @brief Get supported drop actions.
   * @return Supported Qt::DropActions.
   */
  [[nodiscard]] auto supportedDropActions() const -> Qt::DropActions override;

  /**
   * @brief Get supported drag actions.
   * @return Supported Qt::DropActions.
   */
  [[nodiscard]] auto supportedDragActions() const -> Qt::DropActions override;

  /**
   * @brief Get item flags for index.
   * @param index Model index.
   * @return Item flags.
   */
  [[nodiscard]] auto flags(const QModelIndex &index) const
      -> Qt::ItemFlags override;

  /**
   * @brief Get supported MIME types for drag/drop.
   * @return List of MIME type strings.
   */
  [[nodiscard]] auto mimeTypes() const -> QStringList override;

  /**
   * @brief Create MIME data from indexes.
   * @param indexes List of selected indexes.
   * @return MIME data object.
   */
  [[nodiscard]] auto mimeData(const QModelIndexList &indexes) const
      -> QMimeData * override;

  /**
   * @brief Check if drop is possible.
   * @param data MIME data to drop.
   * @param action Drop action.
   * @param row Target row.
   * @param column Target column.
   * @param parent Target parent index.
   * @return true if drop is possible.
   */
  auto canDropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                       int column, const QModelIndex &parent) const
      -> bool override;

  /**
   * @brief Handle dropped MIME data.
   * @param data MIME data to drop.
   * @param action Drop action.
   * @param row Target row.
   * @param column Target column.
   * @param parent Target parent index.
   * @return true if drop was handled.
   */
  auto dropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                    int column, const QModelIndex &parent) -> bool override;
};

#endif // SRC_STOREMODEL_H_
