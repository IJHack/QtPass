#ifndef STOREMODEL_H
#define STOREMODEL_H

#include <QSortFilterProxyModel>

class StoreModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    StoreModel();

    bool filterAcceptsRow(int, const QModelIndex &) const;
    bool ShowThis(const QModelIndex) const;
};

#endif // STOREMODEL_H
