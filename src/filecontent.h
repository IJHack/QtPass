#ifndef FILECONTENT_H
#define FILECONTENT_H

#include <QList>
#include <QString>
#include <QStringList>

struct NamedValue {
  QString name;
  QString value;
};

/**
 * @brief The NamedValues class is mostly a list of \link NamedValue
 * but also has a method to take a specific NamedValue pair out of the list.
 */
class NamedValues : public QList<NamedValue> {
public:
  NamedValues();
  NamedValues(std::initializer_list<NamedValue> values);

  QString takeValue(const QString &name);
};

class FileContent {
public:
  /**
   * @brief parse parses the given fileContent in a FileContent object.
   * The password is accessible through getPassword.
   * The named value pairs (name: value) are parsed and depeding on the
   * templateFields and allFields parameters accessible through getNamedValues
   * or getRemainingData.
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
  static FileContent parse(const QString &fileContent,
                           const QStringList &templateFields, bool allFields);

  /**
   * @return the password from the parsed file.
   */
  QString getPassword() const;

  /**
   * @return the named values in the file in the order of appearence, with
   * template values first.
   */
  NamedValues getNamedValues() const;

  /**
   * @return the data that is not the password and not in getNamedValues.
   */
  QString getRemainingData() const;

private:
  FileContent(const QString &password, const NamedValues &namedValues,
              const QString &remainingData);

  QString password;
  NamedValues namedValues;
  QString remainingData;
};

#endif // FILECONTENT_H
