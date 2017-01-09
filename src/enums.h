#ifndef ENUMS_H
#define ENUMS_H

/*!
    \class Enums
    \brief Enumerators for configuration items
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
  //  have to be last!!!
  PROCESS_COUNT,
  INVALID
};
}

#endif // ENUMS_H
