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
  /**
   * @brief Enums::characterSet enum
   * 0 All character
   * 1 Alphabetical
   * 2 Alphanumeric
   * 3 Custon (from config)
   */
  enum characterSet {
    ALLCHARS = 0,
    ALPHABETICAL = 1,
    ALPHANUMERIC = 2,
    CUSTOM = 3
  };
};

#endif // ENUMS_H
