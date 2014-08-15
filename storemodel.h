#ifndef STOREMODEL_H
#define STOREMODEL_H

#include <QSortFilterProxyModel>
#include <QFileSystemModel>

class StoreModel : public QSortFilterProxyModel
{
    Q_OBJECT
private:
    QFileSystemModel* fs;

public:
    StoreModel();

    bool filterAcceptsRow(int, const QModelIndex &) const;
    bool ShowThis(const QModelIndex) const;
    void setFSModel(QFileSystemModel *sourceModel);
};

#endif // STOREMODEL_H
