// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_TEMPLATEIO_H_
#define SRC_TEMPLATEIO_H_

#include <QHash>
#include <QString>
#include <QStringList>

/**
 * @class TemplateIO
 * @brief Read and write password-store template configuration.
 *
 * Extracted from Util. Owns the `.templates` INI-style file (named template
 * sections with field lists) and the per-folder `.default_template` lookup
 * used to pick a default template when adding a password.
 */
class TemplateIO {
public:
  /**
   * @brief Read templates from .templates file in password store.
   * @param storePath Path to password store root.
   * @return Hash of template name to field list.
   */
  static auto readTemplates(const QString &storePath)
      -> QHash<QString, QStringList>;
  /**
   * @brief Write templates to .templates file in password store.
   * @param storePath Path to password store root.
   * @param templates Hash of template name to field list.
   * @return true if write succeeded.
   */
  static auto writeTemplates(const QString &storePath,
                             const QHash<QString, QStringList> &templates)
      -> bool;
  /**
   * @brief Get default template for a folder.
   * Looks in folder, then parent folders up to root.
   * @param folderPath Path to folder.
   * @param storePath Path to password store root.
   * @return Template name or empty if none found.
   */
  static auto getFolderTemplate(const QString &folderPath,
                                const QString &storePath) -> QString;

private:
  TemplateIO() = default;
};

#endif // SRC_TEMPLATEIO_H_
