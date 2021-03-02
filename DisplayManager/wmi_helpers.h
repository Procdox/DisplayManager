#pragma once

#include "common.h"

#include <iostream>

#define _WIN32_DCOM

#include <comdef.h>
#include <Wbemidl.h>
# pragma comment(lib, "wbemuuid.lib")

class WMIH_Exception : public std::exception {
  const char * const message;
public:
  WMIH_Exception(const char * reason);
  const char * what() const throw ();
};

class ScopedBSTR {
  BSTR b;
public:
  ScopedBSTR(const OLECHAR* c)
  : b(SysAllocString(c)) {}
  ~ScopedBSTR() {
    SysFreeString(b);
  }
  const BSTR& operator*() const {
    return b;
  }
};

class ObjectWrapper;
class ObjectQueryWrapper;
class ServiceWrapper;

class ObjectWrapper : public MOVEONLY<IWbemClassObject> {
  friend ObjectQueryWrapper;
  friend ServiceWrapper;

  explicit ObjectWrapper(IWbemClassObject* p) : MOVEONLY<IWbemClassObject>(p) {}
public:
  ObjectWrapper() : MOVEONLY<IWbemClassObject>(NULL) {};
  ObjectWrapper(ObjectWrapper&& other) : MOVEONLY<IWbemClassObject>(std::move(other)) {};
  ObjectWrapper& operator=(ObjectWrapper&& other) {
    MOVEONLY<IWbemClassObject>::operator=(std::move(other));
    return *this;
  };

  ~ObjectWrapper() {
    if (d())
      d()->Release();
  }

  operator bool() { return d(); }

  int getInt(LPCWSTR property);
  std::wstring getBSTR(LPCWSTR property);
  std::string getCharArray(LPCWSTR property, const int MaxSize);
  std::string getSizedCharArray(LPCWSTR property, LPCWSTR size_property);
  ObjectWrapper getRef(LPCWSTR property, ServiceWrapper& sw);
  void scanWMIMonitorID();
  void scanUSBHubName();
};

class ObjectQueryWrapper : public MOVEONLY<IEnumWbemClassObject> {
  friend ServiceWrapper;

  explicit ObjectQueryWrapper(IEnumWbemClassObject* p) : MOVEONLY<IEnumWbemClassObject>(p) {}
public:
  ObjectQueryWrapper() = delete;
  ObjectQueryWrapper(ObjectQueryWrapper&& other) : MOVEONLY<IEnumWbemClassObject>(std::move(other)) {};
  ObjectQueryWrapper& operator=(ObjectQueryWrapper&& other) {
    MOVEONLY<IEnumWbemClassObject>::operator=(std::move(other));
    return *this;
  };

  ~ObjectQueryWrapper() {
    if (d())
      d()->Release();
  }

  ObjectWrapper Next();
};

class ServiceWrapper {
  IWbemLocator *pLoc = NULL;
  IWbemServices *pSvc = NULL;
public:
  ~ServiceWrapper();
  ServiceWrapper(const OLECHAR*, HRESULT&);

  ObjectQueryWrapper query(const OLECHAR*, const OLECHAR*, HRESULT&);
  ObjectQueryWrapper query(const BSTR&, const BSTR&, HRESULT&);
  ObjectWrapper getObject(const OLECHAR*, HRESULT&);
  ObjectWrapper getObject(const BSTR&, HRESULT&);
};

