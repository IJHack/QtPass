#ifndef STOREMODEL_H
#define STOREMODEL_H

#include <QSortFilterProxyModel>
#include <QFileSystemModel>
#include <QRegExp>

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
    QVariant data(const QModelIndex &index, int role) const;
};

#endif // STOREMODEL_H
