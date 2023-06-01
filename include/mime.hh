#ifndef IVAN_MIME_HH
#define IVAN_MIME_HH

#include <vector>
#include <utility>

namespace ivan {

struct mime_dict {
  char* m { };
  std::vector<std::pair<const char*,const char*>> dict;

public:
  mime_dict(const char* filename);
  ~mime_dict();

  const char* operator()(const char* key, const char* val = nullptr) noexcept;
};

}

#endif
