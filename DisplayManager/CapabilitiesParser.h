#pragma once
#include <unordered_map>
#include <vector>

class feature {
  std::unordered_map<std::string, feature*> members;
public:
  void add(const std::string& s, feature* f);
  bool has(const std::string& s);
  feature* get(const std::string& s);

  ~feature();
  std::vector<std::string> keys();
};

feature* parseFeatures(const std::string& source);