#ifndef IVAN_URL_HH
#define IVAN_URL_HH

#include <vector>
#include <utility>

namespace ivan {

char* split_query(char* path);

void validate_path(char* path);

std::vector<std::pair<const char*,char*>>
parse_query(char* query);

std::vector<std::pair<const char*,std::vector<char*>>>
parse_query_plus(char* query);

}

#endif
