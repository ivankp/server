#ifndef IVANP_URL_PARSER_HH
#define IVANP_URL_PARSER_HH

#include <string>
#include <string_view>
#include <vector>
#include <map>

namespace ivanp {

struct url_parser {
  std::string path;
  std::map<std::string,std::vector<std::string>> params;

  url_parser(const char* url);
};

void validate_path(std::string_view path);

std::string percent_decode(std::string_view);

}

#endif
