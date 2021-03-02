#pragma once

#include <memory>

#define PIMPL class Data; std::unique_ptr<Data> data; Data& d() { return *data; }; Data const& d() const { return *data; };

class bad_access_exception : public std::exception {
  static const char * message;
public:
  const char * what() const throw () { return message; };
};

template<typename T>
class optional : private std::pair<T, bool> {
  using Base = std::pair<T, bool>;
public:
  optional() : std::pair<T, bool>(T(), false) {}
  optional(const T& other) : std::pair<T, bool>(other, true) {}
  optional(T&& other) : std::pair<T, bool>(std::move(other), true) {}
  optional& operator=(const T& other) {
    std::get<0>(*this) = other;
    Base::second = true;
  }
  optional& operator=(T&& other) {
    std::get<0>(*this) = std::move(other);
    Base::second = true;
  }

  operator bool() const {
    return Base::second;
  }
  const T& operator*() const {
    if (!Base::second)
      throw bad_access_exception();
    return std::get<0>(*this);
  }
  T& operator*() {
    if (!Base::second)
      throw bad_access_exception();
    return std::get<0>(*this);
  }
  const T* operator->() const {
    if (!Base::second)
      throw bad_access_exception();
    return &std::get<0>(*this);
  }
  T* operator->() {
    if (!Base::second)
      throw bad_access_exception();
    return &std::get<0>(*this);
  }
};

struct NONCOPY {
  NONCOPY() = default;
  NONCOPY(const NONCOPY&) = delete;
  NONCOPY operator=(const NONCOPY&) = delete;
};

template<typename wrapped>
class MOVEONLY : public NONCOPY {
  wrapped * data;

protected:
  explicit MOVEONLY(wrapped* p) : data(p) {}
public:
  MOVEONLY() = delete;
  MOVEONLY(MOVEONLY&& other)
    : data(other.data) {
    other.data = NULL;
  };
  MOVEONLY& operator=(MOVEONLY&& other) {
    if (data)
      data->Release();
    data = other.data;
    other.data = NULL;

    return *this;
  };
  wrapped * d() { return data; }
  wrapped const * d() const { return data; }
};