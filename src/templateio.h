// SPDX-FileCopyrightText: 2014 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
/**
 * @file
 * @brief TemplateIO: the store's `.templates` and `.default_template` files
 * and the `# secret:` marker in template text.
 */
#ifndef SRC_TEMPLATEIO_H_
#define SRC_TEMPLATEIO_H_

#include <QHash>
#include <QString>
#include <QStringList>

/**
 * @class TemplateIO
 * @ingroup store
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

  /**
   * @brief The field names a template marks as secret.
   *
   * A line `# secret: pin, cvv` names them, separated by commas; "secret"
   * may be written in any case. It is a comment line, so `pass`, QtPass 1.8
   * and the field list skip it. Names are trimmed and empty ones dropped;
   * every other line is ignored. A marked field is hidden like the password
   * (PasswordDisplayPanel), matched by its whole name in any case.
   * @param templateText The text of a template: the store's `.templates`
   *        file or the template in the settings.
   * @return The marked names in the order they appear.
   */
  static auto secretFieldsIn(const QString &templateText) -> QStringList;

  /**
   * @brief The field names the store's `.templates` file marks as secret,
   * in any of its sections (see secretFieldsIn()).
   * @param storePath Path to password store root.
   * @return The marked names; empty when there is no readable file.
   */
  static auto readSecretFields(const QString &storePath) -> QStringList;

  /**
   * @brief The field names of the template in the settings: one per line,
   * with comment lines (starting with `#`, such as a `# secret:` line) left
   * out. Other lines are kept as they are, empty ones too.
   * @param templateText The template's text, newline-separated.
   * @return The field names.
   */
  static auto fieldsIn(const QString &templateText) -> QStringList;

private:
  TemplateIO() = default;
};

#endif // SRC_TEMPLATEIO_H_
