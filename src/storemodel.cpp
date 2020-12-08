#include "storemodel.h"
#include "qtpasssettings.h"

#include <QDebug>
#include <QRegularExpression>
#include <QMessageBox>
#include <QMimeData>
#include <utility>

QDataStream &
operator<<(QDataStream &out,
           const dragAndDropInfoPasswordStore &dragAndDropInfoPasswordStore) {
  out << dragAndDropInfoPasswordStore.isDir
      << dragAndDropInfoPasswordStore.isFile
      << dragAndDropInfoPasswordStore.path;
  return out;
}

QDataStream &
operator>>(QDataStream &in,
           dragAndDropInfoPasswordStore &dragAndDropInfoPasswordStore) {
  in >> dragAndDropInfoPasswordStore.isDir >>
      dragAndDropInfoPasswordStore.isFile >> dragAndDropInfoPasswordStore.path;
  return in;
}

/**
 * @brief StoreModel::StoreModel
 * SubClass of QSortFilterProxyModel via
 * http://www.qtcentre.org/threads/46471-QTreeView-Filter
 */
StoreModel::StoreModel() { fs = nullptr; }

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
  if (fs == nullptr)
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
    path.replace(QRegularExpression("\\.gpg$"), "");
    retVal = path.contains(filterRegularExpression());
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
  setSourceModel(sourceModel);
  fs = sourceModel;
  store = std::move(passStore);
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
    name.replace(QRegularExpression("\\.gpg$"), "");
    initial_value.setValue(name);
  }

  return initial_value;
}

/**
 * @brief StoreModel::supportedDropActions enable drop.
 * @return
 */
Qt::DropActions StoreModel::supportedDropActions() const {
  return Qt::CopyAction | Qt::MoveAction;
}

/**
 * @brief StoreModel::supportedDragActions enable drag.
 * @return
 */
Qt::DropActions StoreModel::supportedDragActions() const {
  return Qt::CopyAction | Qt::MoveAction;
}

/**
 * @brief StoreModel::flags
 * @param index
 * @return
 */
Qt::ItemFlags StoreModel::flags(const QModelIndex &index) const {
  Qt::ItemFlags defaultFlags = QSortFilterProxyModel::flags(index);

  if (index.isValid()) {
    return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | defaultFlags;
  }
  return Qt::ItemIsDropEnabled | defaultFlags;
}

/**
 * @brief StoreModel::mimeTypes
 * @return
 */
QStringList StoreModel::mimeTypes() const {
  QStringList types;
  types << "application/vnd+qtpass.dragAndDropInfoPasswordStore";
  return types;
}

/**
 * @brief StoreModel::mimeData
 * @param indexes
 * @return
 */
QMimeData *StoreModel::mimeData(const QModelIndexList &indexes) const {
  dragAndDropInfoPasswordStore info;

  QByteArray encodedData;
  // only use the first, otherwise we should enable multiselection
  QModelIndex index = indexes.at(0);
  if (index.isValid()) {
    QModelIndex useIndex = mapToSource(index);

    info.isDir = fs->fileInfo(useIndex).isDir();
    info.isFile = fs->fileInfo(useIndex).isFile();
    info.path = fs->fileInfo(useIndex).absoluteFilePath();
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    stream << info;
  }

  auto *mimeData = new QMimeData();
  mimeData->setData("application/vnd+qtpass.dragAndDropInfoPasswordStore",
                    encodedData);
  return mimeData;
}

/**
 * @brief StoreModel::canDropMimeData
 * @param data
 * @param action
 * @param row
 * @param column
 * @param parent
 * @return
 */
bool StoreModel::canDropMimeData(const QMimeData *data, Qt::DropAction action,
                                 int row, int column,
                                 const QModelIndex &parent) const {
#ifdef QT_DEBUG
  qDebug() << action << row;
#else
  Q_UNUSED(action)
  Q_UNUSED(row)
#endif

  QModelIndex useIndex =
      this->index(parent.row(), parent.column(), parent.parent());
  QByteArray encodedData =
      data->data("application/vnd+qtpass.dragAndDropInfoPasswordStore");
  QDataStream stream(&encodedData, QIODevice::ReadOnly);
  dragAndDropInfoPasswordStore info;
  stream >> info;
  if (!data->hasFormat("application/vnd+qtpass.dragAndDropInfoPasswordStore"))
    return false;

  if (column > 0) {
    return false;
  }

  // you can drop a folder on a folder
  if (fs->fileInfo(mapToSource(useIndex)).isDir() && info.isDir) {
    return true;
  }
  // you can drop a file on a folder
  if (fs->fileInfo(mapToSource(useIndex)).isDir() && info.isFile) {
    return true;
  }
  // you can drop a file on a file
  if (fs->fileInfo(mapToSource(useIndex)).isFile() && info.isFile) {
    return true;
  }

  return false;
}

/**
 * @brief StoreModel::dropMimeData
 * @param data
 * @param action
 * @param row
 * @param column
 * @param parent
 * @return
 */
bool StoreModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                              int row, int column, const QModelIndex &parent) {
  if (!canDropMimeData(data, action, row, column, parent))
    return false;

  if (action == Qt::IgnoreAction) {
    return true;
  }
  QByteArray encodedData =
      data->data("application/vnd+qtpass.dragAndDropInfoPasswordStore");

  QDataStream stream(&encodedData, QIODevice::ReadOnly);
  dragAndDropInfoPasswordStore info;
  stream >> info;
  QModelIndex destIndex =
      this->index(parent.row(), parent.column(), parent.parent());
  QFileInfo destFileinfo = fs->fileInfo(mapToSource(destIndex));
  QFileInfo srcFileInfo = QFileInfo(info.path);
  QDir qdir;
  QString cleanedSrc = QDir::cleanPath(srcFileInfo.absoluteFilePath());
  QString cleanedDest = QDir::cleanPath(destFileinfo.absoluteFilePath());
  if (info.isDir) {
    QDir srcDir = QDir(info.path);
    // dropped dir onto dir
    if (destFileinfo.isDir()) {
      QDir destDir = QDir(cleanedDest).filePath(srcFileInfo.fileName());
      QString cleanedDestDir = QDir::cleanPath(destDir.absolutePath());
      if (action == Qt::MoveAction) {
        QtPassSettings::getPass()->Move(cleanedSrc, cleanedDestDir);
      } else if (action == Qt::CopyAction) {
        QtPassSettings::getPass()->Copy(cleanedSrc, cleanedDestDir);
      }
    }
  } else if (info.isFile) {
    // dropped file onto a directory
    if (destFileinfo.isDir()) {
      if (action == Qt::MoveAction) {
        QtPassSettings::getPass()->Move(cleanedSrc, cleanedDest);
      } else if (action == Qt::CopyAction) {
        QtPassSettings::getPass()->Copy(cleanedSrc, cleanedDest);
      }
    } else if (destFileinfo.isFile()) {
      // dropped file onto a file
      int answer = QMessageBox::question(
          nullptr, tr("force overwrite?"),
          tr("overwrite %1 with %2?").arg(cleanedDest).arg(cleanedSrc),
          QMessageBox::Yes | QMessageBox::No);
      bool force = answer == QMessageBox::Yes;
      if (action == Qt::MoveAction) {
        QtPassSettings::getPass()->Move(cleanedSrc, cleanedDest, force);
      } else if (action == Qt::CopyAction) {
        QtPassSettings::getPass()->Copy(cleanedSrc, cleanedDest, force);
      }
    }
  }
  return true;
}

/**
 * @brief StoreModel::lessThan
 * @param source_left
 * @param source_right
 * @return
 */
bool StoreModel::lessThan(const QModelIndex &source_left,
                          const QModelIndex &source_right) const {
/* matches logic in QFileSystemModelSorter::compareNodes() */
#ifndef Q_OS_MAC
  if (fs && (source_left.column() == 0 || source_left.column() == 1)) {
    bool leftD = fs->isDir(source_left);
    bool rightD = fs->isDir(source_right);

    if (leftD ^ rightD)
      return leftD;
  }
#endif

  return QSortFilterProxyModel::lessThan(source_left, source_right);
}
