#include "error.hh"

// TODO: redo validation
// TODO: parse % encoding
// TODO: parse query params

char* split_query(char* path) {
  char* q = strchr(path,'?');
  if (q) *q = '\0';
  return q;
}

void validate_path(std::string_view path) {
  if (path.empty()) THROW_ERROR("path is empty");
  const char* p = path.data();
  const char* const end = p + path.size();
  char c = *p;
  if (c == '/') THROW_ERROR_CAT("path \"",path,"\" starts with '/'");
  c = *(end-1);
  if (c == '/') THROW_ERROR_CAT("path \"",path,"\" ends with '/'");
  for (; p<end; ++p) {
    c  = *p;
    if (!( // allow only - . / _ 09 AZ az
         ('-'<=c && c<='9')
      || ('A'<=c && c<='Z')
      || ('a'<=c && c<='z')
      || c=='_'
    )) {
      THROW_ERROR_CAT("path \"",path,"\" contains \'",c,'\'');
    } else if (c=='.' && (p==path.data() || *(p-1)=='/')) { // disallow ..
      while (*++p=='.') { }
      if (p==end || *p=='/') THROW_ERROR_CAT("path \"",path,"\" contains dots");
      else --p;
    }
  }
}
