#ifndef IVAN_UTF8_HH
#define IVAN_UTF8_HH

#include <cstddef>

void  replace_invalid_utf8(char* s, const char* end) noexcept;
size_t percent_decode_utf8(char* s, const char* end) noexcept;
unsigned utf8_char_len(const char* s) noexcept;

#endif
