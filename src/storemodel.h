#ifndef STOREMODEL_H_
#define STOREMODEL_H_

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

  bool filterAcceptsRow(int, const QModelIndex &) const override;
  bool ShowThis(const QModelIndex) const;
  void setModelAndStore(QFileSystemModel *sourceModel, QString passStore);
  QVariant data(const QModelIndex &index, int role) const override;
  bool lessThan(const QModelIndex &source_left,
                const QModelIndex &source_right) const override;

  // QAbstractItemModel interface
public:
  Qt::DropActions supportedDropActions() const override;
  Qt::DropActions supportedDragActions() const override;
  Qt::ItemFlags flags(const QModelIndex &index) const override;
  QStringList mimeTypes() const override;
  QMimeData *mimeData(const QModelIndexList &indexes) const override;
  bool canDropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                       int column, const QModelIndex &parent) const override;
  bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                    int column, const QModelIndex &parent) override;
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

#endif // STOREMODEL_H_
