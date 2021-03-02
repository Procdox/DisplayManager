#include "monitors.h"
#include "CapabilitiesParser.h"
#include "wmi_helpers.h"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

#include <QDebug>

#define UNICODE 1
#include <lowlevelmonitorconfigurationapi.h>
#include <physicalmonitorenumerationapi.h>
#include <windows.h>
#include <vector>

#pragma comment(lib, "Dxva2.lib")

template<typename t, DISPLAYCONFIG_DEVICE_INFO_TYPE e>
t getDeviceInfo(LUID adapterid, UINT32 id) {
  t info_struct;
  info_struct.header.type = e;
  info_struct.header.size = sizeof(info_struct);
  info_struct.header.adapterId = adapterid;
  info_struct.header.id = id;
  DisplayConfigGetDeviceInfo(&info_struct.header);
  return info_struct;
};

std::wstring getSourceName(const DISPLAYCONFIG_PATH_INFO& path) {
  return getDeviceInfo
    <DISPLAYCONFIG_SOURCE_DEVICE_NAME, DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME>
    (path.sourceInfo.adapterId,path.sourceInfo.id)
    .viewGdiDeviceName;
}

std::wstring getTargetName(const DISPLAYCONFIG_PATH_INFO& path) {
  return getDeviceInfo
    <DISPLAYCONFIG_TARGET_DEVICE_NAME, DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME>
    (path.targetInfo.adapterId, path.targetInfo.id)
    .monitorFriendlyDeviceName;
}

struct ScopedPhysical {
  const DWORD count;
  PHYSICAL_MONITOR * const p;
  

  ScopedPhysical(const HMONITOR handle)
  : count([&](){
      DWORD c;
      GetNumberOfPhysicalMonitorsFromHMONITOR(handle, &c);
      return c;
    }())
  , p(count > 0 ? new PHYSICAL_MONITOR[count] : nullptr) 
  {
    if(p)
      GetPhysicalMonitorsFromHMONITOR(handle, count, p);
  }
  ~ScopedPhysical() {
    if(p) {
      DestroyPhysicalMonitors(count, p);
      delete p;
    }
  }
  
  PHYSICAL_MONITOR& operator[](int x) const {
    return p[x];
  }
};

class DisplayObject::Data {
  static const char * input_names[];

public:
  //  these are all determinable from the initializing HMONITOR alone

  const HMONITOR handle;
  const std::wstring sourceDeviceName;
  const std::wstring id;
  const std::wstring sub_id;

  // determined by path matching
  bool path_found = false;
  std::wstring targetDeviceName;

  // determined by wmi matching
  bool serial_found = false;
  std::string serial;


  ~Data() {}
  Data(HMONITOR _handle) 
  : handle(_handle)
  , sourceDeviceName([&](){
      MONITORINFOEXW info;
      info.cbSize = sizeof(info);
      GetMonitorInfoW(handle, &info);
      return std::wstring(info.szDevice); }())
  , id([&](){
      DISPLAY_DEVICE display;
      ZeroMemory(&display, sizeof(display));
      display.cb = sizeof(display); 

      if( !EnumDisplayDevices(sourceDeviceName.c_str(), 0, &display, 0) )
        throw;
      return std::wstring(display.DeviceID); }()) 
  , sub_id([&](){
      const int start = id.find(L'\\',0) + 1;
      const int end = id.find(L'\\',start);
      return id.substr(start, end - start); }())
  {}

  //getting capabilities is VERY expensive
  sourceList getInputSources() const {
    sourceList result;
    const auto full_start = std::chrono::high_resolution_clock::now();
    auto accum = full_start - full_start;

    ScopedPhysical physicals(handle);
    for (DWORD i = 0; i < physicals.count; i++) {
      
      auto physical = physicals[i].hPhysicalMonitor;
      DWORD cchStringLength = 0;

      const auto start = std::chrono::high_resolution_clock::now();

      if (!GetCapabilitiesStringLength(physical, &cchStringLength))
        continue;

      const auto end = std::chrono::high_resolution_clock::now();
      accum += (end - start);

      // Allocate the string buffer.
      LPSTR szCapabilitiesString = (LPSTR)malloc(cchStringLength);
      // Get the capabilities string.

      
      CapabilitiesRequestAndCapabilitiesReply(physical, szCapabilitiesString, cchStringLength);
      
      

      //parse features and add inputs
      feature* top = parseFeatures(std::string(szCapabilitiesString)); //TODO avoid string
      feature* modes = top->get(std::string("vcp"))->get(std::string("60"));
      for(auto mode : modes->keys()) {
        int index = std::stoi(mode, 0, 16);
        if(index > 18) index = 0;
        result.push_back(std::make_pair(input_names[index],mode));
      }

      delete top;
      free(szCapabilitiesString);
    }

    const auto full_end = std::chrono::high_resolution_clock::now();
    //qDebug() << "Total: " << std::chrono::duration<double>(full_end - full_start).count();
    //qDebug() << "Query: " << std::chrono::duration<double>(accum).count();
    return result;
  }


  std::vector<DWORD> getVCP(BYTE code) const {
    std::vector<DWORD> result;
    ScopedPhysical physicals(handle);
    for (DWORD i = 0; i < physicals.count; i++) {
      auto physical = physicals[i].hPhysicalMonitor;
      DWORD current = 0, max = 0;
      std::cout << physical << (GetVCPFeatureAndVCPFeatureReply(physical, code, NULL, &current, &max) ? " vcp get" : " vcp fail") << std::endl;
      if (code == 0x60)
        current = current % 256;
      result.push_back(current);
    }
    return result;
  }
  bool setVCP(BYTE code, DWORD value) const {
    bool result;
    ScopedPhysical physicals(handle);
    for (DWORD i = 0; i < physicals.count; i++) {
      auto physical = physicals[i].hPhysicalMonitor;
      result &= (bool)SetVCPFeature(physical, code, value);
    }
    return result;
  }
  void debugDisplay() const {
    std::wcout << sourceDeviceName.c_str() << " - " << targetDeviceName.c_str() << std::endl;
  }
};

const char * DisplayObject::Data::input_names[] = {
  "None"
, "Analog 1"
, "Analog 2"
, "DVI 1"
, "DVI 2"
, "Composite 1"
, "Composite 2"
, "S-video 1"
, "S-video 2"
, "Tuner 1"
, "Tuner 2"
, "Tuner 3"
, "Component 1"
, "Component 2"
, "Component 3"
, "DisplayPort 1"
, "DisplayPort 2"
, "HDMI 1"
, "HDMI 2"
};


// ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~
//     DisplayObject
// ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~


DisplayObject::~DisplayObject() {}
DisplayObject::DisplayObject(void* hMonitor)
  : data(std::make_unique<Data>((HMONITOR)hMonitor))
{}
void DisplayObject::debugDisplay() const {
  d().debugDisplay();
}
DisplayObject::DisplayObject(DisplayObject&& other) noexcept {
  data = std::move(other.data);
}
DisplayObject& DisplayObject::operator=(DisplayObject&& other) noexcept {
  data = std::move(other.data);
  return *this;
}
const std::wstring& DisplayObject::name() const {
  return d().targetDeviceName;
}

DisplayObject::sourceList DisplayObject::sources() const {
  return d().getInputSources();
}

std::string DisplayObject::current() const {
  std::stringstream result;
  auto sources = d().getVCP(0x60);

  if(!sources.empty())
    result << std::setw(2) << std::setfill('0') << std::uppercase << std::hex << sources.at(0);

  return result.str();
}

void DisplayObject::setInput(const std::string& s) const {
  if(current() == s)
    return;
    
  int num = std::stoi(s, 0, 16);
  d().setVCP(0x60, num);
}

const std::string& DisplayObject::serial() const {
  return d().serial;
}


// ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~
//     DisplayManager
// ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~


BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
  devices* result = reinterpret_cast<devices*>(dwData);
  result->push_back(std::move(DisplayObject(hMonitor)));
  return TRUE;
}

void determinePaths(devices& data) {
  UINT32 requiredPaths, requiredModes;
  GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, &requiredModes);
  std::vector<DISPLAYCONFIG_PATH_INFO> paths(requiredPaths);
  std::vector<DISPLAYCONFIG_MODE_INFO> modes(requiredModes);
  QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &requiredPaths, paths.data(), &requiredModes, modes.data(), nullptr);
  for (auto& p : paths) {
    const auto sourceName = getSourceName(p);
    bool unique = true;
    
    for(auto& d : data) {
      if( d.d().sourceDeviceName == sourceName ) {
        if( d.d().path_found || !unique )
          throw;
        unique = false;
        d.d().path_found = true;
        d.d().targetDeviceName = getTargetName(p);
      }
    }
    if( unique )
      throw;
  }

  for(auto& d : data) {
    if( !d.d().path_found )
      throw;
  }
}

void determineWMI(devices& data) {
  HRESULT hres;

  try {
    ServiceWrapper helper(L"\\\\.\\root\\wmi", hres);

    auto query = helper.query(L"WQL",L"SELECT * FROM WmiMonitorID", hres);

    if (FAILED(hres)) throw WMIH_Exception("Query failed.");

    ObjectWrapper obj;
    int i = 0;
    std::cout << "Instance ID - Serial ID - Device Name" << std::endl;
    while (obj = query.Next()) {

      const auto id = obj.getBSTR(L"InstanceName");
      const auto serial = obj.getCharArray(L"SerialNumberID", 14);
      if( id.empty() || serial.empty() )
        throw;

      const int start = id.find(L'\\',0) + 1;
      const int end = id.find(L'\\',start);
      const auto sub_id = id.substr(start, end - start);

      bool unique = true;
    
      for(auto& d : data) {
        if( d.d().sub_id == sub_id ) {
          if( d.d().serial_found || !unique )
            throw;
          unique = false;
          d.d().serial_found = true;
          d.d().serial = serial;
        }
      }
    }
  }
  catch (WMIH_Exception& e) {
    std::cout << e.what() << " Error code = 0x" << std::hex << hres << std::endl;
    std::cout << _com_error(hres).ErrorMessage() << std::endl;
  }

  for(auto& d : data) {
    if( !d.d().serial_found )
      throw;
  }
}

const devices& DisplayCollection::get() const {
  return data;
}

void DisplayCollection::refresh() {
  data.clear();
  EnumDisplayMonitors(NULL, NULL, &MonitorEnumProc, reinterpret_cast<LPARAM>(&data));
  determinePaths(data);
  determineWMI(data);
}