// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "storemodel.h"
#include "qtpasssettings.h"

#include "pathvalidator.h"
#include "util.h"
#include <QApplication>
#include <QDebug>
#include <QFileSystemModel>
#include <QMessageBox>
#include <QMimeData>
#include <QRegularExpression>
#include <QtGlobal>

auto operator<<(
    QDataStream &out,
    const dragAndDropInfoPasswordStore &dragAndDropInfoPasswordStore)
    -> QDataStream & {
  out << static_cast<quint8>(dragAndDropInfoPasswordStore.kind)
      << dragAndDropInfoPasswordStore.path;
  return out;
}

auto operator>>(QDataStream &in,
                dragAndDropInfoPasswordStore &dragAndDropInfoPasswordStore)
    -> QDataStream & {
  quint8 k;
  in >> k >> dragAndDropInfoPasswordStore.path;
  switch (k) {
  case static_cast<quint8>(dragAndDropInfoPasswordStore::ItemKind::Directory):
    dragAndDropInfoPasswordStore.kind =
        dragAndDropInfoPasswordStore::ItemKind::Directory;
    break;
  case static_cast<quint8>(dragAndDropInfoPasswordStore::ItemKind::File):
    dragAndDropInfoPasswordStore.kind =
        dragAndDropInfoPasswordStore::ItemKind::File;
    break;
  default:
    dragAndDropInfoPasswordStore.kind =
        dragAndDropInfoPasswordStore::ItemKind::Unknown;
    break;
  }
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
 * StoreModel::showThis method.
 * @param sourceRow
 * @param sourceParent
 * @return
 */
auto StoreModel::filterAcceptsRow(int sourceRow,
                                  const QModelIndex &sourceParent) const
    -> bool {
  QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
  return showThis(index);
}

/**
 * @brief StoreModel::showThis should a row be shown, based on our search
 * criteria.
 * @param index
 * @return
 */
auto StoreModel::showThis(const QModelIndex &index) const -> bool {
  bool retVal = false;
  if (fs == nullptr) {
    return retVal;
  }
  // Gives you the info for number of children with a parent
  if (sourceModel()->rowCount(index) > 0) {
    for (int nChild = 0; nChild < sourceModel()->rowCount(index); ++nChild) {
      QModelIndex childIndex = sourceModel()->index(nChild, 0, index);
      if (!childIndex.isValid()) {
        break;
      }
      retVal = showThis(childIndex);
      if (retVal) {
        break;
      }
    }
  } else {
    QModelIndex useIndex = sourceModel()->index(index.row(), 0, index.parent());
    QString path = fs->filePath(useIndex);
    path = QDir(store).relativeFilePath(path);
    if (path.startsWith(".git")) {
      return false;
    }
    path.replace(Util::endsWithGpg(), "");
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
                                  const QString &passStore) {
  setSourceModel(sourceModel);
  fs = sourceModel;
  store = passStore;
}

auto StoreModel::rootIndexFor(const QString &path) -> QModelIndex {
  if (fs == nullptr) {
    return {};
  }
  return mapFromSource(fs->setRootPath(path));
}

void StoreModel::setStore(const QString &passStore) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  // beginFilterChange() is available since Qt 6.9, but the Direction-scoped
  // endFilterChange(QSortFilterProxyModel::Direction) overload is only
  // available since Qt 6.10, which is the preferred API for scoped and more
  // efficient filter updates.
  beginFilterChange();
  store = passStore;
  endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
  // Direction-scoped filter changes are unavailable before Qt 6.10, so older
  // Qt versions must manually invalidate filters. We update the store and
  // manually invalidate the filter as a compatibility path.
  store = passStore;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
  QSortFilterProxyModel::invalidateFilter();
#else
  invalidateFilter();
#endif
#endif
}

/**
 * @brief StoreModel::data don't show the .gpg at the end of a file.
 * @param index
 * @param role
 * @return
 */
auto StoreModel::data(const QModelIndex &index, int role) const -> QVariant {
  if (!index.isValid()) {
    return {};
  }

  QVariant initial_value;
  initial_value = QSortFilterProxyModel::data(index, role);

  if (role == Qt::DisplayRole) {
    QString name = initial_value.toString();
    name.replace(Util::endsWithGpg(), "");
    initial_value.setValue(name);
  }

  return initial_value;
}

/**
 * @brief StoreModel::supportedDropActions enable drop.
 * @return
 */
auto StoreModel::supportedDropActions() const -> Qt::DropActions {
  return Qt::CopyAction | Qt::MoveAction;
}

/**
 * @brief StoreModel::supportedDragActions enable drag.
 * @return
 */
auto StoreModel::supportedDragActions() const -> Qt::DropActions {
  return Qt::CopyAction | Qt::MoveAction;
}

/**
 * @brief StoreModel::flags
 * @param index
 * @return
 */
auto StoreModel::flags(const QModelIndex &index) const -> Qt::ItemFlags {
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
auto StoreModel::mimeTypes() const -> QStringList {
  QStringList types;
  types << "application/vnd.qtpass.dragAndDropInfoPasswordStore";
  return types;
}

/**
 * @brief StoreModel::mimeData
 * @param indexes
 * @return
 */
auto StoreModel::mimeData(const QModelIndexList &indexes) const -> QMimeData * {
  dragAndDropInfoPasswordStore info;

  QByteArray encodedData;
  // only use the first, otherwise we should enable multiselection
  QModelIndex index = indexes.at(0);
  if (index.isValid()) {
    QModelIndex useIndex = mapToSource(index);
    const QFileInfo fileInfo = fs->fileInfo(useIndex);

    if (fileInfo.isDir()) {
      info.kind = dragAndDropInfoPasswordStore::ItemKind::Directory;
    } else if (fileInfo.isFile()) {
      info.kind = dragAndDropInfoPasswordStore::ItemKind::File;
    }
    info.path = fileInfo.absoluteFilePath();
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    stream << info;
  }

  auto *mimeData = new QMimeData();
  mimeData->setData("application/vnd.qtpass.dragAndDropInfoPasswordStore",
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
auto StoreModel::canDropMimeData(const QMimeData *data, Qt::DropAction action,
                                 int row, int column,
                                 const QModelIndex &parent) const -> bool {
#ifdef QT_DEBUG
  qDebug() << action << row;
#else
  Q_UNUSED(action)
  Q_UNUSED(row)
#endif

  if (data == nullptr ||
      !data->hasFormat("application/vnd.qtpass.dragAndDropInfoPasswordStore")) {
    return false;
  }

  QByteArray encodedData =
      data->data("application/vnd.qtpass.dragAndDropInfoPasswordStore");
  if (encodedData.isEmpty()) {
    return false;
  }
  QDataStream stream(&encodedData, QIODevice::ReadOnly);
  dragAndDropInfoPasswordStore info;
  stream >> info;
  if (stream.status() != QDataStream::Ok) {
    return false;
  }

  QModelIndex useIndex =
      this->index(parent.row(), parent.column(), parent.parent());

  if (column > 0) {
    return false;
  }

  using IK = dragAndDropInfoPasswordStore::ItemKind;
  // you can drop a folder on a folder
  if (fs->fileInfo(mapToSource(useIndex)).isDir() &&
      info.kind == IK::Directory) {
    return true;
  }
  // you can drop a file on a folder
  if (fs->fileInfo(mapToSource(useIndex)).isDir() && info.kind == IK::File) {
    return true;
  }
  // you can drop a file on a file
  if (fs->fileInfo(mapToSource(useIndex)).isFile() && info.kind == IK::File) {
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
auto StoreModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                              int row, int column, const QModelIndex &parent)
    -> bool {
  if (!canDropMimeData(data, action, row, column, parent)) {
    return false;
  }

  if (action == Qt::IgnoreAction) {
    return true;
  }

  if (action != Qt::MoveAction && action != Qt::CopyAction) {
    return false;
  }

  dragAndDropInfoPasswordStore info;
  if (!parseDropData(data, &info)) {
    return false;
  }

  return executeDropAction(info, action, parent);
}

auto StoreModel::parseDropData(const QMimeData *data,
                               dragAndDropInfoPasswordStore *outInfo) -> bool {
  QByteArray encodedData =
      data->data("application/vnd.qtpass.dragAndDropInfoPasswordStore");
  if (encodedData.isEmpty()) {
    return false;
  }

  QDataStream stream(&encodedData, QIODevice::ReadOnly);
  dragAndDropInfoPasswordStore info;
  stream >> info;
  if (stream.status() != QDataStream::Ok) {
    return false;
  }

  *outInfo = info;
  return true;
}

auto StoreModel::executeDropAction(const dragAndDropInfoPasswordStore &info,
                                   Qt::DropAction action,
                                   const QModelIndex &parent) -> bool {
  QModelIndex destIndex =
      this->index(parent.row(), parent.column(), parent.parent());
  QFileInfo destFileinfo = fs->fileInfo(mapToSource(destIndex));
  QFileInfo srcFileInfo = QFileInfo(info.path);

  QString cleanedSrc = QDir::cleanPath(srcFileInfo.absoluteFilePath());
  QString cleanedDest = QDir::cleanPath(destFileinfo.absoluteFilePath());

  // Both endpoints must resolve inside the password store after symlink
  // resolution. Drop data is encoded by the dragged item but could be
  // crafted; canonical-path checks stop drops that would move/copy outside
  // the store or follow a symlink out (e.g. a symlink within the store
  // pointing at /etc).
  const QString storeRoot = QtPassSettings::getPassStore();
  if (!PathValidator::isPathInStore(storeRoot, cleanedSrc) ||
      !PathValidator::isPathInStore(storeRoot, cleanedDest)) {
    qWarning() << "executeDropAction: rejecting drop that escapes the store"
               << "(src=" << cleanedSrc << "dest=" << cleanedDest << ")";
    return false;
  }

  switch (info.kind) {
  case dragAndDropInfoPasswordStore::ItemKind::Directory: {
    // Dropping a folder onto a folder: move/copy it *into* the target.
    if (!destFileinfo.isDir()) {
      return false;
    }
    const QString destDir =
        QDir::cleanPath(QDir(cleanedDest).filePath(srcFileInfo.fileName()));
    return performDrop(cleanedSrc, destDir, action, false);
  }
  case dragAndDropInfoPasswordStore::ItemKind::File:
    // File onto a folder drops into it (no clash); file onto an existing
    // file asks before overwriting.
    if (destFileinfo.isDir()) {
      return performDrop(cleanedSrc, cleanedDest, action, false);
    }
    return performDrop(
        cleanedSrc, cleanedDest, action,
        QMessageBox::question(
            qobject_cast<QWidget *>(QObject::parent()), tr("Force overwrite?"),
            tr("overwrite %1 with %2?").arg(cleanedDest, cleanedSrc),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes);
  default:
    qWarning() << "executeDropAction: unexpected ItemKind, ignoring drop";
    return false;
  }
}

auto StoreModel::performDrop(const QString &cleanedSrc,
                             const QString &cleanedDest, Qt::DropAction action,
                             bool force) -> bool {
  if (action == Qt::MoveAction) {
    QtPassSettings::getPass()->Move(cleanedSrc, cleanedDest, force);
  } else if (action == Qt::CopyAction) {
    QtPassSettings::getPass()->Copy(cleanedSrc, cleanedDest, force);
  }
  return true;
}

/**
 * @brief StoreModel::lessThan
 * @param source_left
 * @param source_right
 * @return
 */
auto StoreModel::lessThan(const QModelIndex &source_left,
                          const QModelIndex &source_right) const -> bool {
/* matches logic in QFileSystemModelSorter::compareNodes() */
#ifndef Q_OS_MAC
  if (fs && (source_left.column() == 0 || source_left.column() == 1)) {
    bool leftD = fs->isDir(source_left);
    bool rightD = fs->isDir(source_right);

    if (leftD ^ rightD) {
      return leftD;
    }
  }
#endif

  return QSortFilterProxyModel::lessThan(source_left, source_right);
}
