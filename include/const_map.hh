#ifndef IVAN_CONST_MAP_HH
#define IVAN_CONST_MAP_HH

#include <utility>
#include <array>
#include <algorithm>
#include <stdexcept>

namespace ivan {

template <typename Key, typename Val, size_t N>
class const_map;

template <typename Key, typename Val, size_t N>
constexpr const_map<Key,Val,N> make_const_map(std::pair<Key,Val> (&&)[N]);

template <typename Key, typename Val, size_t N>
class const_map {
  std::array<Key,N> keys;
  std::array<Val,N> vals;

  constexpr const_map() = default;

public:
  friend constexpr const_map make_const_map<Key,Val,N>(std::pair<Key,Val> (&&)[N]);

  constexpr const Val& operator[](const Key& key) const {
    const auto a = std::begin(keys);
    const auto b = std::end  (keys);
    auto it = std::lower_bound(a, b, key);
    if (it == b || *it != key)
      throw std::out_of_range("const_map: invalid key");
    return vals[it-a];
  }
};

template <typename Key, typename Val, size_t N>
constexpr const_map<Key,Val,N> make_const_map(std::pair<Key,Val> (&&init)[N]) {
  std::sort(init,init+N);
  const_map<Key,Val,N> map;
  for (size_t i=0; i<N; ++i) {
    map.keys[i] = init[i].first;
    map.vals[i] = init[i].second;
  }
  return map;
}

}

#endif
