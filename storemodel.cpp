#include "storemodel.h"

/**
 * @brief StoreModel::StoreModel
 * SubClass of QSortFilterProxyModel via http://www.qtcentre.org/threads/46471-QTreeView-Filter
 */
StoreModel::StoreModel()
{
}

/**
 * @brief StoreModel::filterAcceptsRow
 * @param sourceRow
 * @param sourceParent
 * @return
 */
bool StoreModel::filterAcceptsRow(int sourceRow,
                                           const QModelIndex &sourceParent) const
{
    QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
    return ShowThis(index);
}

/**
 * @brief StoreModel::ShowThis
 * @param index
 * @return
 */
bool StoreModel::ShowThis(const QModelIndex index) const
{
    bool retVal = false;
    //Gives you the info for number of childs with a parent
    if ( sourceModel()->rowCount(index) > 0 )
    {
        for( int nChild = 0; nChild < sourceModel()->rowCount(index); nChild++)
        {
            QModelIndex childIndex = sourceModel()->index(nChild,0,index);
            if ( ! childIndex.isValid() )
                break;
            retVal = ShowThis(childIndex);
            if (retVal)
            {
                break;
            }
        }
    }
    else
    {
        QModelIndex useIndex = sourceModel()->index(index.row(), 0, index.parent());
        QString path = fs->filePath(useIndex);
        path.replace(".gpg", "");
        path.replace(store, "");
        if ( ! path.contains(filterRegExp()))
        {
            retVal = false;
        }
        else
        {
            retVal = true;
        }
    }
    return retVal;
}

void StoreModel::setModelAndStore(QFileSystemModel *sourceModel, QString passStore) {
    fs = sourceModel;
    store = passStore;
}
