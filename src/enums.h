// SPDX-FileCopyrightText: 2016 Anne Jan Brouwer
// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef SRC_ENUMS_H_
#define SRC_ENUMS_H_

/**
 * @namespace Enums
 * @brief Enumerators for QtPass configuration and runtime operations.
 */
namespace Enums {

/**
 * @enum clipBoardType
 * @brief Defines when to copy passwords to clipboard.
 */
enum clipBoardType {
  CLIPBOARD_NEVER = 0,    /**< Never automatically copy to clipboard */
  CLIPBOARD_ALWAYS = 1,   /**< Always copy to clipboard after showing */
  CLIPBOARD_ON_DEMAND = 2 /**< Copy only when user explicitly requests */
};

/**
 * @enum PROCESS
 * @brief Identifies different subprocess operations used in QtPass.
 */
enum PROCESS {
  GIT_INIT = 0,      /**< Initialize Git repository */
  GIT_ADD,           /**< Git add command */
  GIT_COMMIT,        /**< Git commit */
  GIT_RM,            /**< Git remove */
  GIT_PULL,          /**< Git pull */
  GIT_PUSH,          /**< Git push */
  PASS_SHOW,         /**< Pass show - decrypt password file */
  PASS_INSERT,       /**< Pass insert - create/update password */
  PASS_REMOVE,       /**< Pass remove - delete password */
  PASS_INIT,         /**< Pass init - initialize store */
  GPG_GENKEYS,       /**< GPG key generation */
  PASS_MOVE,         /**< Move password file */
  PASS_COPY,         /**< Copy password file */
  GIT_MOVE,          /**< Git move/rename */
  GIT_COPY,          /**< Git copy */
  PASS_OTP_GENERATE, /**< Generate OTP code */
  PASS_GREP,         /**< Search inside password content */
  PROCESS_COUNT,     /**< Total number of process types */
  INVALID            /**< Invalid/unknown process */
};

} // namespace Enums

#endif // SRC_ENUMS_H_
