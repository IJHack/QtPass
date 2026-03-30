// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_STOREMODEL_H_
#define SRC_STOREMODEL_H_

#include <QSortFilterProxyModel>

/*!
    \class StoreModel
    \brief The QSortFilterProxyModel for handling filesystem searches.
 */
class QFileSystemModel;
class StoreModel : public QSortFilterProxyModel {
  Q_OBJECT

private:
  QFileSystemModel *fs;
  QString store;

public:
  StoreModel();

  [[nodiscard]] auto filterAcceptsRow(int, const QModelIndex &) const
      -> bool override;
  [[nodiscard]] auto showThis(const QModelIndex) const -> bool;
  void setModelAndStore(QFileSystemModel *sourceModel, QString passStore);
  [[nodiscard]] auto data(const QModelIndex &index, int role) const
      -> QVariant override;
  [[nodiscard]] auto lessThan(const QModelIndex &source_left,
                              const QModelIndex &source_right) const
      -> bool override;
  [[nodiscard]] auto getStore() const -> QString { return store; }

  // QAbstractItemModel interface
public:
  [[nodiscard]] auto supportedDropActions() const -> Qt::DropActions override;
  [[nodiscard]] auto supportedDragActions() const -> Qt::DropActions override;
  [[nodiscard]] auto flags(const QModelIndex &index) const
      -> Qt::ItemFlags override;
  [[nodiscard]] auto mimeTypes() const -> QStringList override;
  [[nodiscard]] auto mimeData(const QModelIndexList &indexes) const
      -> QMimeData * override;
  auto canDropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                       int column, const QModelIndex &parent) const
      -> bool override;
  auto dropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                    int column, const QModelIndex &parent) -> bool override;
};
/*!
    \struct dragAndDropInfo
    \brief holds values to share beetween drag and drop on the passwordstorage
   view
 */
struct dragAndDropInfoPasswordStore {
  bool isDir;
  bool isFile;
  QString path;
};

#endif // SRC_STOREMODEL_H_
