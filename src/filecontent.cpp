#include "filecontent.h"

FileContent FileContent::parse(const QString &fileContent,
                               const QStringList &templateFields,
                               bool allFields) {
  QStringList lines = fileContent.split("\n");
  QString password = lines.takeFirst();
  QStringList remainingData;
  NamedValues namedValues;
  for (QString line : lines) {
    if (line.contains(":")) {
      int colon = line.indexOf(':');
      QString name = line.left(colon);
      QString value = line.right(line.length() - colon - 1);
      if ((allFields &&
           !value.startsWith(
               "//")) // if value startswith // colon is probably from a url
          || templateFields.contains(name)) {
        namedValues.append({name.trimmed(), value.trimmed()});
        continue;
      }
    }
    remainingData.append(line);
  }
  return FileContent(password, namedValues, remainingData.join("\n"));
}

QString FileContent::getPassword() const { return this->password; }

NamedValues FileContent::getNamedValues() const { return this->namedValues; }

QString FileContent::getRemainingData() const { return this->remainingData; }

FileContent::FileContent(const QString &password,
                         const NamedValues &namedValues,
                         const QString &remainingData)
    : password(password), namedValues(namedValues),
      remainingData(remainingData) {}

NamedValues::NamedValues() : QList() {}

NamedValues::NamedValues(std::initializer_list<NamedValue> values)
    : QList(values) {}

QString NamedValues::takeValue(const QString &name) {
  for (int i = 0; i < length(); ++i) {
    if (at(i).name == name) {
      return takeAt(i).value;
    }
  }
  return QString();
}
