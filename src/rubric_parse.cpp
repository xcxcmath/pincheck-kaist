#include <iostream>
#include <fstream>
#include <regex>

#include "rubric_parse.h"
#include "execution.h"
#include "string_helper.h"

Vector<Rubric> parse_rubric(const Path& grading_file, Vector<TestCase> &target_tests) {
  using namespace std::string_literals;
  std::ostringstream panic_msg;

  if(target_tests.empty()) return {};

  if(!fs::exists(grading_file)) {
    panic_msg << "The grading file " << grading_file << " doesn't exist";
    panic(panic_msg);
  }

  std::ifstream grading_if(grading_file);
  if(!grading_if.is_open()) {
    panic_msg << "The grading file " << grading_file << " cannot be opened";
    panic(panic_msg);
  }

  Vector<String> parsing_lines;
  String temp;
  while(std::getline(grading_if, temp)) {
    auto commenter = temp.find('#');
    if(commenter != String::npos) {
      temp = temp.substr(0, commenter);
    }
    temp = string_trim(temp);
    if(temp.empty())
      continue;
    
    parsing_lines.emplace_back(temp);
  }
  grading_if.close();

  Vector<Rubric> rubrics;
  for(const auto& line: parsing_lines){
    auto tokens = string_tokenize(line);
    if(tokens.size() != 2)
      continue;
    if(tokens[0].empty() || tokens[0].back() != '%')
      continue;
    tokens[0].pop_back();

    Rubric rubric;
    rubric.max_pct = std::stod(tokens[0]);

    Path rubric_file = tokens[1];
    auto last_comma = tokens[1].find_last_of('.');
    if(last_comma != String::npos){
      rubric.subdir_suffix = tokens[1].substr(last_comma+1);
    }
    rubric.subdir = rubric_file.parent_path();
    rubric_file = Path{"../.."} / rubric_file;

    std::ifstream rubric_if(rubric_file);
    if(!rubric_if.is_open()) {
      panic_msg << "Cannot open " << rubric_file;
      panic(panic_msg);
    }
    bool first_line = true;
    String curr_subtitle;
    while(std::getline(rubric_if, temp)) {
      temp = string_trim(temp);
      if(first_line) {
        first_line = false;
        rubric.title = temp;
      } else {
        if(temp.empty()) continue;

        const auto first_space = std::find_if(temp.cbegin(), temp.cend(), [](char c){
          return std::isspace(c);
        });
        if(first_space == temp.cend()) continue;

        const auto first_token = string_trim(String{temp.cbegin(), first_space});
        const auto other_token = string_trim(String{first_space+1, temp.cend()});
        if(first_token == "-"){
          curr_subtitle = other_token;
        } else {
          auto target = std::find_if(target_tests.begin(), target_tests.end(),
            [&rubric, &other_token](const TestCase &tc){
              return tc.full_name() == rubric.subdir + "/" + other_token;
            });

          if(target == target_tests.end()) continue;

          if(rubric.subtitles.count(curr_subtitle) == 0)
            rubric.subtitles[curr_subtitle] = {};
          rubric.subtitles[curr_subtitle].emplace_back(target->full_name());
          target->max_ptr = std::stoi(first_token);
          target->subtitle = curr_subtitle;
        }
      }
    }
    rubric_if.close();

    rubrics.emplace_back(rubric);
  }

  return rubrics;

}