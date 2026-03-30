// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#include "storemodel.h"
#include <QFileSystemModel>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QString>

constexpr int MS_PER_SECOND = 1000;

class StoreModel;

/*!
    \class Util
    \brief Some static utilities to be used elsewhere.
 */
class Util {
public:
  static auto findBinaryInPath(QString binary) -> QString;
  static auto findPasswordStore() -> QString;
  static auto normalizeFolderPath(QString path) -> QString;
  static auto checkConfig() -> bool;
  static auto getDir(const QModelIndex &index, bool forPass,
                     const QFileSystemModel &model,
                     const StoreModel &storeModel) -> QString;
  static auto endsWithGpg() -> const QRegularExpression &;
  static auto protocolRegex() -> const QRegularExpression &;
  static auto newLinesRegex() -> const QRegularExpression &;

private:
  static void initialiseEnvironment();
  static QProcessEnvironment _env;
  static bool _envInitialised;
};

#endif // SRC_UTIL_H_
