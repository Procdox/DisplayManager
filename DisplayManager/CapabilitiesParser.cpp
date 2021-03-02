#include "CapabilitiesParser.h"
#include <functional>

void feature::add(const std::string& s, feature* f) {
  members.emplace(s, f);
}
bool feature::has(const std::string& s) {
  return members.find(s) != members.end();
}
feature* feature::get(const std::string& s) {
  auto iter = members.find(s);
  if (iter != members.end())
    return iter->second;
  return nullptr;
}

feature::~feature() {
  for (auto member : members) {
    if (member.second) {
      delete member.second;
    }
  }
}
std::vector<std::string> feature::keys() {
  std::vector<std::string> result;
  for (auto pair : members) {
    result.push_back(pair.first);
  }
  return result;
}

feature* parseFeatures(const char* source) {
  throw std::logic_error("not impl");
  return nullptr;
}

feature* parseFeatures(const std::string& source) {
  int spot = 0;
  std::function<feature*()> recurse;
  recurse = [&]() -> feature* {
    spot += 1;
    int start = spot;
    bool content = false;
    feature* result = new feature();
    while (true) {
      if (source[spot] == ' ') {
        if (content) {
          const auto section = source.substr(start, spot - start);
          result->add(section, nullptr);
        }
        content = false;
        spot += 1;
        start = spot;
      }
      else if (source[spot] == '(') {
        const auto title = source.substr(start, spot - start);

        auto* sub = recurse();
        result->add(title, sub);
        content = false;
        start = spot;
      }
      else if (source[spot] == ')') {
        if (content) {
          const auto section = source.substr(start, spot - start);
          result->add(section, nullptr);
        }
        spot += 1;
        return result;
      }
      else {
        content = true;
        spot += 1;
      }

    }
  };
  feature* top = recurse();
  return top;
}