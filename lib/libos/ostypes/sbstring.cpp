#include "sbstring.h"
#include <cstring>

using namespace sb;

String::String() : charSize(cSize), nChars(0), stringData(NULL) {
}

String::String(const char *str) {
  if(!str) {
    charSize = 1;
    stringData = NULL;
    nChars = 0;
    return;
  }

  nChars = strlen(str);
  charSize = 1;
  stringData = new char[nChars * charSize];

  if(!stringData) {
    nChars = 0;
    return;
  }

  memcpy(stringData, str, nChars * charSize);
}

String::String(const String &str) {
  if(str.stringData == NULL) {
    str.stringData = NULL;
    nChars = 0;
    charSize = str.charSize;
  }
  else {
    stringData = new char[str.nChars * str.charSize];
    nChars = str.nChars;
    charSize = str.charSize;

    memcpy(stringData, str.stringData, nChars * charSize);
  }
}

String::~String() {
  delete[] stringData;
}

int String::operator[](int index) const {
  int val;

  if(index >= 0 && index < nChars) {
    memcpy(&val, (void*)((unsigned)stringData + index * charSize), charSize);
    return val;
  }
  else if(index < 0 && index >= -nChars) {
    memcpy(&val, (void*)((unsigned)stringData + (nChars + index) * charSize),
           charSize);
    return val;
  }
  else
    return '\0';
}

int String::operator==(const String &str) const;
{

}

size_t String::length() const {
  return nChars;
}
