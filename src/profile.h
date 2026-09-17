// SPDX-FileCopyrightText: 2026 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_PROFILE_H_
#define SRC_PROFILE_H_

#include <QMap>
#include <QString>
#include <optional>

/**
 * @struct Profile
 * @brief One named password store: where it is, how its .gpg-id is signed
 *        and, optionally, its own Git switches.
 *
 * The Git flags are tri-state on disk ("" / "true" / "false"): unset means
 * "follow the global setting", which is what every profile written before
 * 1.8.0 has. Keys are stored under `profile/<name>/` and are unchanged from
 * the QHash-of-QHash days.
 */
struct Profile {
  QString path;
  QString signingKey;
  std::optional<bool> useGit;
  std::optional<bool> autoPush;
  std::optional<bool> autoPull;

  /**
   * @brief Parse the on-disk tri-state.
   * @param value "true", "false" or anything else (unset).
   * @return The flag, or nullopt when unset.
   */
  static auto flagFromString(const QString &value) -> std::optional<bool> {
    if (value == QLatin1String("true")) {
      return true;
    }
    if (value == QLatin1String("false")) {
      return false;
    }
    return std::nullopt;
  }

  /**
   * @brief Format a flag the way it is stored.
   * @param flag The flag.
   * @return "true", "false" or "" for unset.
   */
  static auto flagToString(std::optional<bool> flag) -> QString {
    if (!flag.has_value()) {
      return {};
    }
    return *flag ? QStringLiteral("true") : QStringLiteral("false");
  }

  /**
   * @brief Field-wise equality, including the unset state of each flag.
   * @param other Profile to compare with.
   * @return true when every field matches.
   */
  auto operator==(const Profile &other) const -> bool {
    return path == other.path && signingKey == other.signingKey &&
           useGit == other.useGit && autoPush == other.autoPush &&
           autoPull == other.autoPull;
  }
  /**
   * @brief Negation of operator==.
   * @param other Profile to compare with.
   * @return true when any field differs.
   */
  auto operator!=(const Profile &other) const -> bool {
    return !(*this == other);
  }
};

/// All profiles by name. QMap so iteration order is the name order.
using Profiles = QMap<QString, Profile>;

#endif // SRC_PROFILE_H_
