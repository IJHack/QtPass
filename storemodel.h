#ifndef STOREMODEL_H_
#define STOREMODEL_H_

#include <QFileSystemModel>
#include <QRegExp>
#include <QSortFilterProxyModel>

class StoreModel : public QSortFilterProxyModel {
  Q_OBJECT
private:
  QFileSystemModel *fs;
  QString store;

public:
  StoreModel();

  bool filterAcceptsRow(int, const QModelIndex &) const;
  bool ShowThis(const QModelIndex) const;
  void setModelAndStore(QFileSystemModel *sourceModel, QString passStore);
  QVariant data(const QModelIndex &index, int role) const;
};

#endif // STOREMODEL_H_
