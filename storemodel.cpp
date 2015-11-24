#include "storemodel.h"

/**
 * @brief StoreModel::StoreModel
 * SubClass of QSortFilterProxyModel via
 * http://www.qtcentre.org/threads/46471-QTreeView-Filter
 */
StoreModel::StoreModel() { fs = NULL; }

/**
 * @brief StoreModel::filterAcceptsRow
 * @param sourceRow
 * @param sourceParent
 * @return
 */
bool StoreModel::filterAcceptsRow(int sourceRow,
                                  const QModelIndex &sourceParent) const {
  QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
  return ShowThis(index);
}

/**
 * @brief StoreModel::ShowThis
 * @param index
 * @return
 */
bool StoreModel::ShowThis(const QModelIndex index) const {
  bool retVal = false;
  if (fs == NULL)
    return retVal;
  // Gives you the info for number of childs with a parent
  if (sourceModel()->rowCount(index) > 0) {
    for (int nChild = 0; nChild < sourceModel()->rowCount(index); ++nChild) {
      QModelIndex childIndex = sourceModel()->index(nChild, 0, index);
      if (!childIndex.isValid())
        break;
      retVal = ShowThis(childIndex);
      if (retVal)
        break;
    }
  } else {
    QModelIndex useIndex = sourceModel()->index(index.row(), 0, index.parent());
    QString path = fs->filePath(useIndex);
    path.replace(QRegExp("\\.gpg$"), "");
    path.replace(QRegExp("^" + store), "");
    retVal = path.contains(filterRegExp());
  }
  return retVal;
}

/**
 * @brief StoreModel::setModelAndStore
 * @param sourceModel
 * @param passStore
 */
void StoreModel::setModelAndStore(QFileSystemModel *sourceModel,
                                  QString passStore) {
  fs = sourceModel;
  store = passStore;
}

/**
 * @brief StoreModel::data
 * @param index
 * @param role
 * @return
 */
QVariant StoreModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid())
    return QVariant();

  QVariant initial_value;
  initial_value = QSortFilterProxyModel::data(index, role);

  if (role == Qt::DisplayRole) {
    QString name = initial_value.toString();
    name.replace(QRegExp("\\.gpg$"), "");
    initial_value.setValue(name);
  }

  return initial_value;
}
