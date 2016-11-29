#include "storemodel.h"
#include <QDebug>
#include <QStringListModel>
#include <QMimeData>


QDataStream &operator<<(QDataStream &out, const QList<dragAndDropInfoPasswordStore> &dragAndDropInfoPasswordStores)
{
    foreach (dragAndDropInfoPasswordStore info, dragAndDropInfoPasswordStores) {
        out << info.isDir
            << info.isFile
            << info.path;
    }
    return out;
}



QDataStream &operator>>(QDataStream &in, QList<dragAndDropInfoPasswordStore> &dragAndDropInfoPasswordStores)
{
    while (in.atEnd() == false){
        dragAndDropInfoPasswordStore info;
        in >> info.isDir
           >> info.isFile
           >> info.path;
        dragAndDropInfoPasswordStores.append(info);
    }
    return in;
}

/**
 * @brief StoreModel::StoreModel
 * SubClass of QSortFilterProxyModel via
 * http://www.qtcentre.org/threads/46471-QTreeView-Filter
 */
StoreModel::StoreModel() { fs = NULL; }

/**
 * @brief StoreModel::filterAcceptsRow should row be shown, wrapper for
 * StoreModel::ShowThis method.
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
 * @brief StoreModel::ShowThis should a row be shown, based on our search
 * criteria.
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
      path = QDir(store).relativeFilePath(path);
      path.replace(QRegExp("\\.gpg$"), "");
      retVal = path.contains(filterRegExp());
  }
  return retVal;
}

/**
 * @brief StoreModel::setModelAndStore update the source model and store.
 * @param sourceModel
 * @param passStore
 */
void StoreModel::setModelAndStore(QFileSystemModel *sourceModel,
                                  QString passStore) {
  fs = sourceModel;
  store = passStore;
}

/**
 * @brief StoreModel::data don't show the .gpg at the end of a file.
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

/**
 * @brief StoreModel::supportedDropActions enable drop.
 * @return
 */
Qt::DropActions StoreModel::supportedDropActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}
/**
 * @brief StoreModel::supportedDragActions enable drag.
 * @return
 */
Qt::DropActions StoreModel::supportedDragActions() const
{
    return Qt::CopyAction | Qt::MoveAction;
}

Qt::ItemFlags StoreModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QSortFilterProxyModel::flags(index);

    if (index.isValid()){
        return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | defaultFlags;
    }else{
        return Qt::ItemIsDropEnabled | defaultFlags;
    }
}

QStringList StoreModel::mimeTypes() const
{
    QStringList types;
    types << "application/vnd+qtpass.dragAndDropInfoPasswordStore";
    return types;
}

QMimeData *StoreModel::mimeData(const QModelIndexList &indexes) const
{
    QList<dragAndDropInfoPasswordStore> infos;

    foreach (const QModelIndex &index, indexes) {
        if (index.isValid()) {
            QModelIndex useIndex  = mapToSource(index);

            dragAndDropInfoPasswordStore info;
            info.isDir = fs->fileInfo(useIndex).isDir();
            info.isFile = fs->fileInfo(useIndex).isFile();
            info.path = fs->fileInfo(useIndex).absoluteFilePath();
            infos.append(info);
        }
    }

    QByteArray encodedData;
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    stream << infos;

    QMimeData *mimeData = new QMimeData();
    mimeData->setData("application/vnd+qtpass.dragAndDropInfoPasswordStore", encodedData);
    return mimeData;
}

bool StoreModel::canDropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent) const
{
    QModelIndex useIndex = sourceModel()->index(parent.row(),parent.column(), parent.parent());
    QByteArray encodedData = data->data("application/vnd+qtpass.dragAndDropInfoPasswordStore");
    QDataStream stream(&encodedData, QIODevice::ReadOnly);
    QList<dragAndDropInfoPasswordStore> infos;
    stream >> infos;
    if (!data->hasFormat("application/vnd+qtpass.dragAndDropInfoPasswordStore"))
        return false;

    if (column > 0)
        return false;

    return true;
}

bool StoreModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column, const QModelIndex &parent)
{
    if (!canDropMimeData(data, action, row, column, parent))
        return false;

    if (action == Qt::IgnoreAction){
        return true;
    }
    QByteArray encodedData = data->data("application/vnd+qtpass.dragAndDropInfoPasswordStore");

    QDataStream stream(&encodedData, QIODevice::ReadOnly);
    QList<dragAndDropInfoPasswordStore> infos;
    stream >> infos;
    QModelIndex destIndex = this->index(parent.row(), parent.column(), parent.parent());
    QFileInfo destFileinfo = fs->fileInfo(mapToSource(destIndex));

    QDir qdir;
    foreach (const dragAndDropInfoPasswordStore &info, infos) {
        if(info.isDir){
                QDir srcDir = QDir(info.path);
                if(destFileinfo.isDir()){
                    QString droppedOnDir = destFileinfo.absoluteFilePath();
                    QDir destDir = QDir(droppedOnDir).filePath(srcDir.dirName());
                    QString cleanedSrcDir = qdir.cleanPath(srcDir.absolutePath());
                    QString cleanedDestDir = qdir.cleanPath(destDir.absolutePath());
                    if(action == Qt::MoveAction){
                            QtPassSettings::getPass()->Move(QDir(cleanedSrcDir), QDir(cleanedDestDir));
                    }
                }else if (destFileinfo.isFile()){
                    //@todo nothing to here. show dialog. dont drop directories on files
                }
        }else if(info.isFile){

        }
    }
    return true;
}
