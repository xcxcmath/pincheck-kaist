#include <regex>
#include "string_helper.h"

String string_trim(String s) {
  String result = std::move(s);
  result.erase(result.begin(),
    std::find_if(result.begin(), result.end(), [](int c) {
      return !std::isspace(c);
    }));
  result.erase(std::find_if(result.rbegin(), result.rend(), [](int c) {
    return !std::isspace(c);
    }).base(), result.end());
  return result;
}

Vector<String> string_tokenize(String line) {
  const String target = string_trim(std::move(line));
  const auto re = std::regex{R"(\s+)"};
  return Vector<String>(
    std::sregex_token_iterator(target.cbegin(), target.cend(), re, -1),
    std::sregex_token_iterator()
  );
}

bool wildcard_match(const String &target, const String &pattern) {
  const auto PSZ = pattern.size(), TSZ = target.size();
  Vector<Vector<unsigned>> dp(2, Vector<unsigned>(PSZ+1));

  dp[0][0] = true;
  for(size_t pi = 1; pi <= PSZ; ++pi) {
    dp[0][pi] = dp[0][pi-1] && (pattern[pi-1] == '*');
  }
  for(size_t ti = 1; ti <= TSZ; ++ti) {
    const auto tc = target[ti-1];
    auto& row = dp[ti&1];
    const auto& row_prev = dp[!(ti&1)];
    row[0] = false;

    for(size_t pi = 1; pi <= PSZ; ++pi) {
      const auto pc = pattern[pi-1];
      if(pc == '*') {
        row[pi] = row_prev[pi] || row[pi-1];
      } else if (tc == pc || pc == '?'){
        row[pi] = row_prev[pi-1];
      } else {
        row[pi] = false;
      }
    }
  }

  return dp[TSZ&1][PSZ];
}

bool wildcard_match(const String &target, const Vector<String> &patterns) {
  return std::any_of(patterns.cbegin(), patterns.cend(), [&target](const String& pat){
    return wildcard_match(target, pat);
  });
}