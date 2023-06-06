#ifndef IVAN_URL_HH
#define IVAN_URL_HH

#include <vector>
#include <utility>

char* split_query(char* path);

std::vector<std::pair<const char*,const char*>>
parse_query(char* query);

std::vector<std::pair<const char*,const char*>>
parse_query_sort(char* query);

#endif
