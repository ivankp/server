#include "utf8.hh"
#include <cstring>

unsigned utf8_char_len(const char* s) noexcept {
  const char c = *s;
  if ((c & 0b1110'0000) == 0b1100'0000) {
    return 2;
  } else if ((c & 0b1111'0000) == 0b1110'0000) {
    return 3;
  } else if ((c & 0b1111'1000) == 0b1111'0000) {
    return 4;
  } else {
    return 1;
  }
}

void replace_invalid_utf8(char* s, const char* e) noexcept {
  // replace non-utf-8 bytes with spaces
  for (unsigned n=0, p=0; s!=e; ++s) {
    const char c = *s;
    if (n==0) {
same_char: ;
      if ((c & 0b1000'0000) == 0b0000'0000) { // ascii
        continue;
      } else if ((c & 0b1110'0000) == 0b1100'0000) {
        n = 2;
      } else if ((c & 0b1111'0000) == 0b1110'0000) {
        n = 3;
      } else if ((c & 0b1111'1000) == 0b1111'0000) {
        n = 4;
      } else { // invalid utf-8
        *s = ' ';
        continue;
      }
    } else {
      if ((c & 0b1100'0000) != 0b1000'0000) { // invalid utf-8
        while (p) s[--p] = ' ';
        n = 0;
        goto same_char;
      }
    }
    ++p;
    if (p==n) {
      p = n = 0;
    } else if (s==e) { // p>0 here
      while (p) s[--p] = ' ';
      break;
    }
  }
}

size_t percent_decode_utf8(char* s, const char* end) noexcept {
  const char *r = s, *r1 = nullptr;
  char* w = s;
  char c, cc;
  char point[4] { };
  unsigned n = 0, p = 0;
  while (r != end) { // decode %-encoded valid utf-8
    c = *r++;
    if (c!='%' || end-r < 2) [[likely]] {
      if (r1) [[unlikely]] goto dump;
      *w++ = c;
      continue;
    }
    if (!r1) r1 = r-1;

    c = *r++;
    if ('0' <= c && c <= '9') { c -= '0'   ; } else
    if ('A' <= c && c <= 'Z') { c -= 'A'-10; } else
    if ('a' <= c && c <= 'z') { c -= 'a'-10; } else
    goto dump;
    cc = c << 4;

    c = *r++;
    if ('0' <= c && c <= '9') { c -= '0'   ; } else
    if ('A' <= c && c <= 'Z') { c -= 'A'-10; } else
    if ('a' <= c && c <= 'z') { c -= 'a'-10; } else
    goto dump;
    cc += c;

    if (n==0) {
same_char: ;
      if ((cc & 0b1000'0000) == 0b0000'0000) { // ascii
        *w++ = cc;
        r1 = nullptr;
        continue;
      } else if ((cc & 0b1110'0000) == 0b1100'0000) {
        n = 2;
      } else if ((cc & 0b1111'0000) == 0b1110'0000) {
        n = 3;
      } else if ((cc & 0b1111'1000) == 0b1111'0000) {
        n = 4;
      } else { // invalid utf-8
        goto dump;
      }
    } else {
      if ((cc & 0b1100'0000) != 0b1000'0000) { // invalid utf-8
        const size_t nc = (r-3) - r1;
        memmove(w,r1,nc);
        w += nc;
        r1 += nc;
        p = 0;
        goto same_char;
      }
    }
    point[p++] = cc;

    if (p==n) {
      memmove(w,point,n);
      w += n;
      r1 = nullptr;
      n = 0;
      p = 0;
    }

    continue;

dump:
    const size_t nc = r - r1;
    memmove(w,r1,nc);
    w += nc;
    r1 = nullptr;
    n = 0;
    p = 0;
  }

  return w-s;
}
