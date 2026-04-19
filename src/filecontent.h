// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_FILECONTENT_H_
#define SRC_FILECONTENT_H_

#include <QList>
#include <QString>
#include <QStringList>

/**
 * @struct NamedValue
 * @brief A name/value pair parsed from a password file field.
 */
struct NamedValue {
  /**
   * @brief Field name (the part before the colon in "name: value").
   */
  QString name;
  /**
   * @brief Field value (the part after the colon in "name: value").
   */
  QString value;
};

inline bool operator==(const NamedValue &a, const NamedValue &b) {
  return a.name == b.name && a.value == b.value;
}

/**
 * @brief The NamedValues class is mostly a list of NamedValue
 * but also has a method to take a specific NamedValue pair out of the list.
 */
class NamedValues : public QList<NamedValue> {
public:
  NamedValues();
  /**
   * @brief Construct a NamedValues list from an initializer list.
   * @param values Initial set of NamedValue pairs.
   */
  NamedValues(std::initializer_list<NamedValue> values);

  /**
   * @brief Remove and return the value for the named field.
   * @param name Field name to look up and remove.
   * @return The value of the removed field, or an empty string if not found.
   */
  auto takeValue(const QString &name) -> QString;
};

/**
 * @class FileContent
 * @brief Represents the parsed contents of a password file.
 *
 * Splits the file into a password line, named key/value fields, and
 * remaining data. Use FileContent::parse to construct an instance.
 */
class FileContent {
public:
  /**
   * @brief parse parses the given fileContent in a FileContent object.
   * The password is accessible through getPassword.
   * The named value pairs (name: value) are parsed and depeding on the
   * templateFields and allFields parameters accessible through getNamedValues,
   * getRemainingData or getRemainingDataForDisplay.
   *
   * @param fileContent the file content to parse.
   *
   * @param templateFields the fields in the template.
   * Fields in the template will always be in getNamedValues() at the beginning
   * of the list in the same order.
   *
   * @param allFields whether all fields should be considered as named values.
   * If set to false only templateFields are returned in getNamedValues().
   *
   * @return
   */
  static auto parse(const QString &fileContent,
                    const QStringList &templateFields, bool allFields)
      -> FileContent;

  /**
   * @return the password from the parsed file.
   */
  [[nodiscard]] auto getPassword() const -> QString;

  /**
   * @return the named values in the file in the order of appearence, with
   * template values first.
   */
  [[nodiscard]] auto getNamedValues() const -> NamedValues;

  /**
   * @return the data that is not the password and not in getNamedValues.
   */
  [[nodiscard]] auto getRemainingData() const -> QString;

  /**
   * @brief Like getRemainingData but without data that should not be displayed
   * (like a TOTP secret).
   * @return Remaining data string safe for display.
   */
  [[nodiscard]] auto getRemainingDataForDisplay() const -> QString;

private:
  FileContent(QString password, NamedValues namedValues, QString remainingData,
              QString remainingDataDisplay);

  QString password;
  NamedValues namedValues;
  QString remainingData, remainingDataDisplay;
};

#endif // SRC_FILECONTENT_H_
