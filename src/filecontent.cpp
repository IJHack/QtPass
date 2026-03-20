// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#include "filecontent.h"

#include "helpers.h"
#include <utility>

static auto isLineHidden(const QString &line) -> bool {
  return line.startsWith("otpauth://", Qt::CaseInsensitive);
}

auto FileContent::parse(const QString &fileContent,
                        const QStringList &templateFields, bool allFields)
    -> FileContent {
  QStringList lines = fileContent.split("\n");
  QString password = lines.takeFirst();
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

auto FileContent::getPassword() const -> QString { return this->password; }

auto FileContent::getNamedValues() const -> NamedValues {
  return this->namedValues;
}

auto FileContent::getRemainingData() const -> QString {
  return this->remainingData;
}

auto FileContent::getRemainingDataForDisplay() const -> QString {
  return this->remainingDataDisplay;
}

FileContent::FileContent(QString password, NamedValues namedValues,
                         QString remainingData, QString remainingDataDisplay)
    : password(std::move(password)), namedValues(std::move(namedValues)),
      remainingData(std::move(remainingData)),
      remainingDataDisplay(std::move(remainingDataDisplay)) {}

NamedValues::NamedValues() = default;

NamedValues::NamedValues(std::initializer_list<NamedValue> values)
    : QList(values) {}

auto NamedValues::takeValue(const QString &name) -> QString {
  for (int i = 0; i < length(); ++i) {
    if (at(i).name == name) {
      return takeAt(i).value;
    }
  }
  return {};
}
