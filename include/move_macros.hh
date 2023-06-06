#ifndef IVAN_MOVE_MACROS_HH
#define IVAN_MOVE_MACROS_HH

#define MOV(...) \
  static_cast<std::remove_reference_t<decltype(__VA_ARGS__)>&&>(__VA_ARGS__)

#define FWD(...) \
  static_cast<decltype(__VA_ARGS__)&&>(__VA_ARGS__)

#endif
