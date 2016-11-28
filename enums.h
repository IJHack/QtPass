#ifndef ENUMS_H
#define ENUMS_H

class Enums {
public:
  /**
   * @brief Enums::clipBoardType enum
   * 0 Never
   * 1 Always
   * 2 On demand
   */
  enum clipBoardType {
    CLIPBOARD_NEVER = 0,
    CLIPBOARD_ALWAYS = 1,
    CLIPBOARD_ON_DEMAND = 2
  };
};

#endif // ENUMS_H
