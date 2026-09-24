// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "storemodel.h"
#include "pass.h"
#include "pathvalidator.h"
#include "qtpasslogging.h"
#include "util.h"
#include <QApplication>
#include <QDebug>
#include <QFileSystemModel>
#include <QMessageBox>
#include <QMimeData>
#include <QRegularExpression>
#include <QtGlobal>

auto operator<<(QDataStream &out, const StoreDragItem &item) -> QDataStream & {
  out << static_cast<quint8>(item.kind) << item.path;
  return out;
}

auto operator>>(QDataStream &in, StoreDragItem &item) -> QDataStream & {
  quint8 k;
  in >> k >> item.path;
  switch (k) {
  case static_cast<quint8>(StoreDragItem::ItemKind::Directory):
    item.kind = StoreDragItem::ItemKind::Directory;
    break;
  case static_cast<quint8>(StoreDragItem::ItemKind::File):
    item.kind = StoreDragItem::ItemKind::File;
    break;
  default:
    item.kind = StoreDragItem::ItemKind::Unknown;
    break;
  }
  return in;
}

// Filter approach via http://www.qtcentre.org/threads/46471-QTreeView-Filter
StoreModel::StoreModel() { fs = nullptr; }

void StoreModel::setPass(Pass *pass) { m_pass = pass; }

auto StoreModel::filterAcceptsRow(int sourceRow,
                                  const QModelIndex &sourceParent) const
    -> bool {
  QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
  return showThis(index);
}

auto StoreModel::showThis(const QModelIndex &index) const -> bool {
  bool retVal = false;
  if (fs == nullptr) {
    return retVal;
  }
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
  return mapFromSource(fs->setRootPath(QDir::cleanPath(path)));
}

void StoreModel::setStore(const QString &passStore) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
  // The Direction-scoped endFilterChange() overload needs Qt 6.10.
  beginFilterChange();
  store = passStore;
  endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
  store = passStore;
  invalidateFilter();
#endif
}

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

auto StoreModel::supportedDropActions() const -> Qt::DropActions {
  return Qt::CopyAction | Qt::MoveAction;
}

auto StoreModel::supportedDragActions() const -> Qt::DropActions {
  return Qt::CopyAction | Qt::MoveAction;
}

auto StoreModel::flags(const QModelIndex &index) const -> Qt::ItemFlags {
  Qt::ItemFlags defaultFlags = QSortFilterProxyModel::flags(index);

  if (index.isValid()) {
    return Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled | defaultFlags;
  }
  return Qt::ItemIsDropEnabled | defaultFlags;
}

auto StoreModel::mimeTypes() const -> QStringList {
  QStringList types;
  types << kStoreDragMimeType;
  return types;
}

auto StoreModel::mimeData(const QModelIndexList &indexes) const -> QMimeData * {
  StoreDragItem info;

  if (indexes.isEmpty())
    return nullptr;
  QByteArray encodedData;
  // only use the first, otherwise we should enable multiselection
  QModelIndex index = indexes.at(0);
  if (index.isValid()) {
    QModelIndex useIndex = mapToSource(index);
    const QFileInfo fileInfo = fs->fileInfo(useIndex);

    if (fileInfo.isDir()) {
      info.kind = StoreDragItem::ItemKind::Directory;
    } else if (fileInfo.isFile()) {
      info.kind = StoreDragItem::ItemKind::File;
    }
    info.path = fileInfo.absoluteFilePath();
    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    stream << info;
  }

  auto *mimeData = new QMimeData();
  mimeData->setData(kStoreDragMimeType, encodedData);
  return mimeData;
}

auto StoreModel::canDropMimeData(const QMimeData *data, Qt::DropAction action,
                                 int row, int column,
                                 const QModelIndex &parent) const -> bool {
  qCDebug(lcQtPass) << "canDropMimeData" << action << row;

  const auto parsed = parseDropData(data);
  if (!parsed) {
    return false;
  }
  const StoreDragItem &info = *parsed;

  QModelIndex useIndex =
      this->index(parent.row(), parent.column(), parent.parent());

  if (column > 0) {
    return false;
  }

  using IK = StoreDragItem::ItemKind;
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

  const auto info = parseDropData(data);
  if (!info) {
    return false;
  }

  return executeDropAction(*info, action, parent);
}

auto StoreModel::parseDropData(const QMimeData *data)
    -> std::optional<StoreDragItem> {
  if (data == nullptr || !data->hasFormat(kStoreDragMimeType)) {
    return std::nullopt;
  }
  QByteArray encodedData = data->data(kStoreDragMimeType);
  if (encodedData.isEmpty()) {
    return std::nullopt;
  }
  QDataStream stream(&encodedData, QIODevice::ReadOnly);
  StoreDragItem info;
  stream >> info;
  if (stream.status() != QDataStream::Ok) {
    return std::nullopt;
  }
  return info;
}

auto StoreModel::executeDropAction(const StoreDragItem &info,
                                   Qt::DropAction action,
                                   const QModelIndex &parent) -> bool {
  QModelIndex destIndex =
      this->index(parent.row(), parent.column(), parent.parent());
  QFileInfo destFileinfo = fs->fileInfo(mapToSource(destIndex));
  QFileInfo srcFileInfo = QFileInfo(info.path);

  QString cleanedSrc = QDir::cleanPath(srcFileInfo.absoluteFilePath());
  QString cleanedDest = QDir::cleanPath(destFileinfo.absoluteFilePath());

  // Drop data could be crafted: both endpoints must resolve inside the store
  // after symlink resolution (e.g. no in-store symlink pointing at /etc).
  if (!PathValidator::isPathInStore(store, cleanedSrc) ||
      !PathValidator::isPathInStore(store, cleanedDest)) {
    qCWarning(lcQtPass)
        << "executeDropAction: rejecting drop that escapes the store"
        << "(src=" << cleanedSrc << "dest=" << cleanedDest << ")";
    return false;
  }

  switch (info.kind) {
  case StoreDragItem::ItemKind::Directory: {
    // Dropping a folder onto a folder: move/copy it *into* the target.
    if (!destFileinfo.isDir()) {
      return false;
    }
    const QString destDir =
        QDir::cleanPath(QDir(cleanedDest).filePath(srcFileInfo.fileName()));
    return performDrop(cleanedSrc, destDir, action, false);
  }
  case StoreDragItem::ItemKind::File:
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
    qCWarning(lcQtPass)
        << "executeDropAction: unexpected ItemKind, ignoring drop";
    return false;
  }
}

auto StoreModel::performDrop(const QString &cleanedSrc,
                             const QString &cleanedDest, Qt::DropAction action,
                             bool force) -> bool {
  if (!m_pass) {
    return false;
  }
  if (action == Qt::MoveAction) {
    m_pass->Move(cleanedSrc, cleanedDest, force);
  } else if (action == Qt::CopyAction) {
    m_pass->Copy(cleanedSrc, cleanedDest, force);
  }
  return true;
}

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
