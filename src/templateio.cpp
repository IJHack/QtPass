// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later

/**
 * @class TemplateIO
 * @brief Read and write password-store template configuration implementation.
 *
 * @see templateio.h
 */

#include "templateio.h"
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <algorithm>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

auto TemplateIO::readTemplates(const QString &storePath)
    -> QHash<QString, QStringList> {
  QHash<QString, QStringList> result;
  QFile file(QDir(storePath).filePath(".templates"));
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return result;
  }
  QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
  in.setEncoding(QStringConverter::Utf8);
#else
  in.setCodec("UTF-8");
#endif
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
    if (dir.exists(".default_template")) {
      QFile file(dir.filePath(".default_template"));
      if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        in.setEncoding(QStringConverter::Utf8);
#else
        in.setCodec("UTF-8");
#endif
        QString templateName = in.readLine().trimmed();
        file.close();
        if (!templateName.isEmpty() && !templateName.startsWith('#')) {
          return templateName;
        }
      }
    }
    QString currentPath = QDir::cleanPath(dir.absolutePath());
    if (currentPath == cleanStoreAbs) {
      break;
    }
    if (!currentPath.startsWith(cleanStoreAbs) ||
        currentPath.length() <= cleanStoreAbs.length() ||
        currentPath.at(cleanStoreAbs.length()) != QChar('/')) {
      break;
    }
    if (!dir.cdUp()) {
      break;
    }
  }
  return {};
}
