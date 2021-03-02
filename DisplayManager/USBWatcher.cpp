#include "USBWatcher.h"

#include "wmi_helpers.h"

#include <regex>
#include <QDebug>

hubList getConnectedHubs() {
  HRESULT hres;
  hubList result;

  ServiceWrapper helper(L"\\\\.\\root\\CIMV2", hres);

  ObjectWrapper obj;
  auto query = helper.query(L"WQL",L"SELECT * FROM CIM_USBHub", hres);
  if (FAILED(hres)) throw WMIH_Exception("Query failed.");

  while (obj = query.Next()) {
    const auto deviceID = obj.getBSTR(L"DeviceID");
    const auto deviceDesc = obj.getBSTR(L"Description");
    if( deviceID.empty() || deviceDesc.empty() )
      throw;
    
    result.push_back(std::make_pair(deviceID, deviceDesc));
  }

  return result;
}

std::wstring double_escape(const std::wstring& source) {
  std::wstring result = source; 
  int o = 0;
  int s = 0;
  while(true){
    int i = result.find(L'\\',o);
    if(i == result.npos)
      break;

    result.replace(i,0, L"\\");
    o = i + 2;
    if(++s >= 10)
      throw;
  }

  return result;
}


bool isHubConnected(const std::wstring& queryID) {
  HRESULT hres;

  ServiceWrapper helper(L"\\\\.\\root\\CIMV2", hres);
  ObjectWrapper obj;

  const std::wstring query_string = L"SELECT * FROM CIM_USBHub WHERE DeviceID='" + double_escape(queryID) + L"'";
  auto query = helper.query(L"WQL",query_string.c_str(), hres);
  if (FAILED(hres)) throw WMIH_Exception("Query failed.");
  
  return (obj = query.Next());
}


hubList getConnectedUSB() {
  HRESULT hres;
  hubList result;

  ServiceWrapper helper(L"\\\\.\\root\\CIMV2", hres);
  ObjectWrapper main_obj;
  
  auto main_query = helper.query(L"WQL",L"SELECT * FROM Win32_USBControllerDevice", hres);
  if (FAILED(hres)) throw WMIH_Exception("Query failed.");

  while (main_obj = main_query.Next()) {

    auto deviceID = main_obj.getBSTR(L"Dependent");
    auto device_obj = main_obj.getRef(L"Dependent", helper);
    auto deviceDesc = device_obj.getBSTR(L"Description");
    if( deviceID.empty() || !device_obj || deviceDesc.empty() )
      throw;

    result.push_back(std::make_pair(deviceID, deviceDesc));
  }

  return result;
}

bool isUSBConnected(const std::wstring& queryID) {
  HRESULT hres;

  ServiceWrapper helper(L"\\\\.\\root\\CIMV2", hres);

  return (bool)helper.getObject(queryID.c_str(), hres);
}