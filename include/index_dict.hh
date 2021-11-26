#ifndef IVANP_INDEX_DICT_HH
#define IVANP_INDEX_DICT_HH

#include <cstdlib>
#include <algorithm>
#include <type_traits>

namespace ivanp {

template <typename T>
requires std::is_trivially_copyable_v<T>
class index_dict {
  T* m;
  size_t n;

public:
  index_dict(size_t n)
  requires ( std::is_trivially_default_constructible_v<T>)
  : m( reinterpret_cast<T*>(::calloc( n, sizeof(T) )) ), n(n)
  { }
  index_dict(size_t n)
  requires (!std::is_trivially_default_constructible_v<T>)
  : m( reinterpret_cast<T*>(::malloc( n*sizeof(T) )) ), n(n)
  {
    new (m) T[n] { };
  }
  ~index_dict() {
    if constexpr (!std::is_trivially_destructible_v<T>)
      std::for_each( m, m+n, [](auto& x){ x.~T(); } );
    ::free(m);
  }

  void resize(size_t n2) {
    if (n < n2) {
      m = reinterpret_cast<T*>(::realloc( m, n2*sizeof(T) ));
      new (m+n) T[n2-n] { };
    } else if (n2 < n) {
      if constexpr (!std::is_trivially_destructible_v<T>)
        std::for_each( m+n2, m+n, [](auto& x){ x.~T(); } );
      m = reinterpret_cast<T*>(::realloc( m, n2*sizeof(T) ));
    }
    n = n2;
  }

  T& operator[](size_t i) noexcept {
    if (i >= n) {
      size_t s = i; // compute next power of 2
      for (size_t p=1; p<(sizeof(size_t)*8); p <<= 1)
        s |= s >> p;
      ++s;
      m = reinterpret_cast<T*>(::realloc( m, s*sizeof(T) )); // resize up
      new (m+n) T[s-n] { };
      n = s;
    }
    return m[i];
  }
  T operator[](size_t i) const noexcept {
    return i<n ? m[i] : T{};
  }
};

} // end namespace ivanp

#endif
