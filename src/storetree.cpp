// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "storetree.h"
#include "util.h"
#include <QDir>
#include <QHeaderView>
#include <QTreeView>

StoreTree::StoreTree(QTreeView *view, QObject *parent)
    : QObject(parent), m_view(view) {
  m_fs.setNameFilters(QStringList() << QStringLiteral("*.gpg"));
  m_fs.setNameFilterDisables(false);

  m_view->setModel(&m_proxy);
  m_view->setHeaderHidden(true);
  m_view->setIndentation(15);
  m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_view->setContextMenuPolicy(Qt::CustomContextMenu);
  m_view->header()->setSectionResizeMode(0, QHeaderView::Stretch);
  m_view->sortByColumn(0, Qt::AscendingOrder);
}

void StoreTree::setStore(const QString &path) {
  m_store = path;
  const QModelIndex rootDir = m_fs.setRootPath(path);
  m_fs.fetchMore(rootDir);
  if (m_proxy.sourceModel() == nullptr) {
    m_proxy.setModelAndStore(&m_fs, path);
    // Columns exist only once a source model is attached; hide the size,
    // type and date ones now (a no-op before that point).
    m_view->setColumnHidden(1, true);
    m_view->setColumnHidden(2, true);
    m_view->setColumnHidden(3, true);
  } else {
    m_proxy.setStore(path);
  }
  m_proxy.setFilterRegularExpression(QRegularExpression());
  m_view->setRootIndex(m_proxy.rootIndexFor(path));
}

void StoreTree::setPass(Pass *pass) { m_proxy.setPass(pass); }

void StoreTree::setFilter(const QRegularExpression &pattern) {
  m_proxy.setFilterRegularExpression(pattern);
  // Filtering re-derives the mapping; the root must be looked up again.
  m_view->setRootIndex(m_proxy.rootIndexFor(m_store));
}

auto StoreTree::dirFor(const QModelIndex &index, bool forPass) const
    -> QString {
  const QString abspath = QDir(m_store).absolutePath() + QDir::separator();
  if (!index.isValid()) {
    return forPass ? QString() : abspath;
  }
  const QFileInfo info = m_fs.fileInfo(m_proxy.mapToSource(index));
  QString filePath =
      info.isFile() ? info.absolutePath() : info.absoluteFilePath();
  if (forPass) {
    filePath = QDir(abspath).relativeFilePath(filePath);
  }
  filePath += QDir::separator();
  return filePath;
}

auto StoreTree::currentDir(bool forPass) const -> QString {
  return dirFor(m_view->currentIndex(), forPass);
}

auto StoreTree::fileFor(const QModelIndex &index, bool forPass) const
    -> QString {
  if (!index.isValid() || !m_fs.fileInfo(m_proxy.mapToSource(index)).isFile()) {
    return {};
  }
  QString filePath = m_fs.filePath(m_proxy.mapToSource(index));
  if (forPass) {
    filePath = QDir(m_store).relativeFilePath(filePath);
    filePath.replace(Util::endsWithGpg(), "");
  }
  return filePath;
}

auto StoreTree::currentFile(bool forPass) const -> QString {
  return fileFor(m_view->currentIndex(), forPass);
}

auto StoreTree::fileInfo(const QModelIndex &index) const -> QFileInfo {
  return m_fs.fileInfo(m_proxy.mapToSource(index));
}

auto StoreTree::indexFor(const QString &path) const -> QModelIndex {
  const QModelIndex src = m_fs.index(path);
  return src.isValid() ? m_proxy.mapFromSource(src) : QModelIndex();
}

auto StoreTree::rootIndex() -> QModelIndex {
  return m_proxy.rootIndexFor(m_store);
}

auto StoreTree::currentIndex() const -> QModelIndex {
  return m_view->currentIndex();
}
