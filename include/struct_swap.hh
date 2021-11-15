#ifndef IVANP_STRUCT_SWAP_HH
#define IVANP_STRUCT_SWAP_HH

#include <type_traits>
#include <cstring>

namespace ivanp {

template <typename T>
void struct_swap(T& a, T& b) noexcept {
  typename std::aligned_storage<sizeof(T),alignof(T)>::type m;
  memcpy(&m,&a,sizeof(T));
  memcpy(&a,&b,sizeof(T));
  memcpy(&b,&m,sizeof(T));
}

}

#endif
