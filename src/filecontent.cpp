// SPDX-FileCopyrightText: 2018 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "filecontent.h"

#include "helpers.h"
#include <utility>

/**
 * @brief Checks if a line should be hidden from display.
 * @param line The line to check
 * @return true if the line starts with otpauth:// (case-insensitive)
 */
static auto isLineHidden(const QString &line) -> bool {
  return line.startsWith("otpauth://", Qt::CaseInsensitive);
}

/**
 * @brief Parses password file content into structured data.
 * @param fileContent Raw file content
 * @param templateFields Fields defined in the template
 * @param allFields Whether to include all name:value pairs
 * @return Parsed FileContent object
 */
auto FileContent::parse(const QString &fileContent,
                        const QStringList &templateFields, bool allFields)
    -> FileContent {
  QStringList lines = fileContent.split("\n");
  QString password;
  if (!lines.isEmpty()) {
    password = lines.takeFirst();
  }
  QStringList remainingData;
  QStringList remainingDataDisplay;
  NamedValues namedValues;
  for (const QString &line : AS_CONST(lines)) {
    if (line.contains(":")) {
      int colon = line.indexOf(':');
      QString name = line.left(colon);
      QString value = line.right(line.length() - colon - 1);
      if ((allFields &&
           !value.startsWith(
               "//")) // if value startswith  // colon is probably from a url
          || templateFields.contains(name)) {
        namedValues.append({name.trimmed(), value.trimmed()});
        continue;
      }
    }

    remainingData.append(line);
    if (!isLineHidden(line)) {
      remainingDataDisplay.append(line);
    }
  }
  return {password, namedValues, remainingData.join("\n"),
          remainingDataDisplay.join("\n")};
}

/**
 * @brief Gets the password from the parsed file.
 * @return The password string
 */
auto FileContent::getPassword() const -> QString { return this->password; }

/**
 * @brief Gets named value pairs from the parsed file.
 * @return NamedValues list with name:value pairs
 */
auto FileContent::getNamedValues() const -> NamedValues {
  return this->namedValues;
}

/**
 * @brief Gets remaining data not in named values.
 * @return Remaining data as string
 */
auto FileContent::getRemainingData() const -> QString {
  return this->remainingData;
}

/**
 * @brief Gets remaining data for display (excludes hidden fields like OTP).
 * @return Remaining data suitable for display
 */
auto FileContent::getRemainingDataForDisplay() const -> QString {
  return this->remainingDataDisplay;
}

/**
 * @brief Constructs a FileContent with all parsed data.
 * @param password The password
 * @param namedValues Named value pairs
 * @param remainingData Remaining data not in named values
 * @param remainingDataDisplay Remaining data for display
 */
FileContent::FileContent(QString password, NamedValues namedValues,
                         QString remainingData, QString remainingDataDisplay)
    : password(std::move(password)), namedValues(std::move(namedValues)),
      remainingData(std::move(remainingData)),
      remainingDataDisplay(std::move(remainingDataDisplay)) {}

/**
 * @brief Default constructor for NamedValues.
 */
NamedValues::NamedValues() = default;

/**
 * @brief Constructs NamedValues from initializer list.
 * @param values Initializer list of NamedValue
 */
NamedValues::NamedValues(std::initializer_list<NamedValue> values)
    : QList(values) {}

/**
 * @brief Finds and removes a named value by name.
 * @param name The name to search for
 * @return The value if found, empty string otherwise
 */
auto NamedValues::takeValue(const QString &name) -> QString {
  for (int i = 0; i < length(); ++i) {
    if (at(i).name == name) {
      return takeAt(i).value;
    }
  }
  return {};
}
