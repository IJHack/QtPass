#include "filecontent.h"
#include "helpers.h"

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
    if (!isLineHidden(line))
      remainingDataDisplay.append(line);
  }
  return FileContent(password, namedValues, remainingData.join("\n"),
                     remainingDataDisplay.join("\n"));
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

FileContent::FileContent(const QString &password,
                         const NamedValues &namedValues,
                         const QString &remainingData,
                         const QString &remainingDataDisplay)
    : password(password), namedValues(namedValues),
      remainingData(remainingData), remainingDataDisplay(remainingDataDisplay) {
}

NamedValues::NamedValues() {}

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
