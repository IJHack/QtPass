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
#include <QStringConverter>
#include <QTextStream>
#include <algorithm>
#include <optional>

namespace {

/// The name of a `[section]` header line; nothing for any other line, and
/// an empty name for `[]`.
auto sectionHeader(const QString &line) -> std::optional<QString> {
  if (!line.startsWith('[') || !line.endsWith(']')) {
    return std::nullopt;
  }
  return line.mid(1, line.length() - 2).trimmed();
}

/// The first line of @p dir's .default_template, when it names a template.
auto defaultTemplateIn(const QDir &dir) -> QString {
  QFile file(dir.filePath(".default_template"));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }
  QTextStream in(&file);
  in.setEncoding(QStringConverter::Utf8);
  const QString name = in.readLine().trimmed();
  return name.startsWith('#') ? QString() : name;
}

} // namespace

auto TemplateIO::readTemplates(const QString &storePath)
    -> QHash<QString, QStringList> {
  QHash<QString, QStringList> result;
  QFile file(QDir(storePath).filePath(".templates"));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return result;
  }
  QTextStream in(&file);
  in.setEncoding(QStringConverter::Utf8);
  // No section before the first header, nor for an empty `[]`: fields there
  // belong to nothing and are dropped.
  std::optional<QString> section;
  QStringList fields;
  const auto flush = [&] {
    if (section) {
      result.insert(*section, fields);
    }
    fields.clear();
  };
  while (!in.atEnd()) {
    const QString line = in.readLine().trimmed();
    if (const std::optional<QString> header = sectionHeader(line)) {
      flush();
      section = header->isEmpty() ? std::nullopt : header;
      if (!section) {
        qWarning()
            << "Empty template section in .templates file, ignoring fields";
      }
    } else if (section && !line.isEmpty() && !line.startsWith('#')) {
      fields.append(line);
    }
  }
  flush();
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
  out.setEncoding(QStringConverter::Utf8);
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
  const QString store = QDir::cleanPath(QDir(storePath).absolutePath());
  // Only the store root and its descendants are probed for
  // .default_template; a path outside the store opens nothing.
  const auto inStore = [&store](const QString &path) {
    return path == store ||
           (path.startsWith(store) && path.length() > store.length() &&
            path.at(store.length()) == QChar('/'));
  };
  QDir dir(folderPath);
  while (inStore(QDir::cleanPath(dir.absolutePath()))) {
    const QString name = defaultTemplateIn(dir);
    if (!name.isEmpty()) {
      return name;
    }
    if (QDir::cleanPath(dir.absolutePath()) == store || !dir.cdUp()) {
      break;
    }
  }
  return {};
}
