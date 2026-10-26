// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @class TemplateIO
 * @brief Read and write password-store template configuration implementation.
 *
 * @see templateio.h
 */

#include "templateio.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <algorithm>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif
#include <QFileInfo>
#include <optional>

namespace {

/// The template files are a few lines each; anything larger is not one.
constexpr qint64 kMaxTemplateFileBytes = 64 * 1024;

/**
 * Reads a template file of the store. Store contents are written by whoever
 * shares the store, so it has to be a regular file (not a link, a device or
 * a FIFO, which may never end) of a template file's size; at most that much
 * is read, so a file that grows meanwhile is cut off there.
 * @return The contents, or nothing when the file is absent, not a regular
 *         file or too large.
 */
auto readTemplateFile(const QString &path) -> std::optional<QString> {
  const QFileInfo info(path);
  if (info.isSymLink() || !info.isFile()) {
    return std::nullopt;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return std::nullopt;
  }
  const QByteArray bytes = file.read(kMaxTemplateFileBytes + 1);
  if (bytes.size() > kMaxTemplateFileBytes) {
    qWarning() << "Ignoring" << path << "; a template file is at most"
               << kMaxTemplateFileBytes << "bytes";
    return std::nullopt;
  }
  // A UTF-8 byte order mark (older Notepad writes one) is not text.
  const QByteArray bom("\xEF\xBB\xBF");
  return QString::fromUtf8(bytes.startsWith(bom) ? bytes.mid(bom.size())
                                                 : bytes);
}

} // namespace

auto TemplateIO::readTemplates(const QString &storePath)
    -> QHash<QString, QStringList> {
  QHash<QString, QStringList> result;
  std::optional<QString> contents =
      readTemplateFile(QDir(storePath).filePath(".templates"));
  if (!contents) {
    return result;
  }
  QTextStream in(&*contents, QIODevice::ReadOnly);
  QString currentSection;
  QStringList currentFields;
  bool skipInvalidSection = false;
  while (!in.atEnd()) {
    QString line = in.readLine().trimmed();
    if (line.startsWith('[') && line.endsWith(']')) {
      if (!currentSection.isEmpty() && !skipInvalidSection) {
        result.insert(currentSection, currentFields);
      }
      currentSection = line.mid(1, line.length() - 2).trimmed();
      if (currentSection.isEmpty()) {
        qWarning()
            << "Empty template section in .templates file, ignoring fields";
        skipInvalidSection = true;
        currentFields.clear();
      } else {
        skipInvalidSection = false;
        currentFields.clear();
      }
    } else if (!line.isEmpty() && !line.startsWith('#') &&
               !skipInvalidSection) {
      currentFields.append(line);
    }
  }
  if (!currentSection.isEmpty() && !skipInvalidSection) {
    result.insert(currentSection, currentFields);
  }
  return result;
}

auto TemplateIO::writeTemplates(const QString &storePath,
                                const QHash<QString, QStringList> &templates)
    -> bool {
  QSaveFile saveFile(QDir(storePath).filePath(".templates"));
  if (!saveFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return false;
  }
  QTextStream out(&saveFile);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  out.setEncoding(QStringConverter::Utf8);
#else
  out.setCodec("UTF-8");
#endif
  out << "# QtPass templates configuration\n";
  out << "# Format: INI-style with [template_name] sections,\n";
  out << "# followed by field names (one per line)\n\n";

  QStringList sortedKeys = templates.keys();
  std::sort(sortedKeys.begin(), sortedKeys.end());
  for (const QString &key : sortedKeys) {
    out << "[" << key << "]\n";
    for (const QString &field : templates.value(key)) {
      out << field << "\n";
    }
    out << "\n";
  }
  out.flush();
  if (out.status() != QTextStream::Ok) {
    return false;
  }
  return saveFile.commit();
}

auto TemplateIO::getFolderTemplate(const QString &folderPath,
                                   const QString &storePath) -> QString {
  QDir storeDir(storePath);
  QString cleanStoreAbs = QDir::cleanPath(storeDir.absolutePath());
  QDir dir(folderPath);
  while (true) {
    QString currentPath = QDir::cleanPath(dir.absolutePath());
    const bool atStoreRoot = currentPath == cleanStoreAbs;
    const bool insideStore =
        currentPath.startsWith(cleanStoreAbs) &&
        currentPath.length() > cleanStoreAbs.length() &&
        currentPath.at(cleanStoreAbs.length()) == QChar('/');
    // Reject paths outside the store before opening any file; only the store
    // root and its descendants are probed for .default_template.
    if (!atStoreRoot && !insideStore) {
      break;
    }
    if (dir.exists(".default_template")) {
      const std::optional<QString> contents =
          readTemplateFile(dir.filePath(".default_template"));
      if (contents) {
        const QString templateName =
            contents->section(QLatin1Char('\n'), 0, 0).trimmed();
        if (!templateName.isEmpty() && !templateName.startsWith('#')) {
          return templateName;
        }
      }
    }
    if (atStoreRoot) {
      break;
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return {};
}
