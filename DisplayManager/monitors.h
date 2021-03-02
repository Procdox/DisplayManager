#pragma once

#include "common.h"
#include <vector>

struct DisplayObject {
  PIMPL
  
  using sourceList = std::vector<std::pair<std::string, std::string>>;

  ~DisplayObject();
  DisplayObject(void*);

  DisplayObject(const DisplayObject&) = delete;
  DisplayObject& operator=(const DisplayObject&) = delete;
  DisplayObject(DisplayObject&&) noexcept;
  DisplayObject& operator=(DisplayObject&&) noexcept;

  //monitor API stuff
  void debugDisplay() const;
  const std::wstring& name() const;
  sourceList sources() const;
  std::string current() const;
  void setInput(const std::string&) const;

  //WQL stuff
  const std::string& serial() const;
};

using devices = std::vector<DisplayObject>;

class DisplayCollection {
  devices data;
public:
  const devices& get() const;
  void refresh();
};
