#ifndef PINCHECK_STRING_HELPER_H
#define PINCHECK_STRING_HELPER_H

#include "common.h"

String string_trim(String s);
Vector<String> string_tokenize(String line);
bool wildcard_match(const String &target, const String &pattern);
bool wildcard_match(const String &target, const Vector<String> &patterns);

#endif