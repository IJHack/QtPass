#ifndef STOREMODEL_H
#define STOREMODEL_H

#include <QSortFilterProxyModel>
#include <QFileSystemModel>

class StoreModel : public QSortFilterProxyModel
{
    Q_OBJECT
private:
    QFileSystemModel* fs;
    QString store;

public:
    StoreModel();

    bool filterAcceptsRow(int, const QModelIndex &) const;
    bool ShowThis(const QModelIndex) const;
    void setModelAndStore(QFileSystemModel *sourceModel, QString passStore);
};

#endif // STOREMODEL_H
