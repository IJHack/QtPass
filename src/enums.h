#ifndef ENUMS_H
#define ENUMS_H

/*!
    \namespace Enums
    \brief     Enumerators for configuration and runtime items
*/
namespace Enums {

enum clipBoardType {
  CLIPBOARD_NEVER = 0,
  CLIPBOARD_ALWAYS = 1,
  CLIPBOARD_ON_DEMAND = 2
};

enum PROCESS {
  GIT_INIT = 0,
  GIT_ADD,
  GIT_COMMIT,
  GIT_RM,
  GIT_PULL,
  GIT_PUSH,
  PASS_SHOW,
  PASS_INSERT,
  PASS_REMOVE,
  PASS_INIT,
  GPG_GENKEYS,
  PASS_MOVE,
  PASS_COPY,
  GIT_MOVE,
  GIT_COPY,
  PROCESS_COUNT,
  INVALID,
  PASS_OTP_GENERATE,

};

} // namespace Enums

#endif // ENUMS_H
