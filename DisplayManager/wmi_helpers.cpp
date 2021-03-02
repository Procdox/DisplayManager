#include "wmi_helpers.h"


// ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~
//     WMIH_Exception
// ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~

WMIH_Exception::WMIH_Exception(const char * reason)
  : message(reason)
{}

const char * WMIH_Exception::what() const throw ()
{
  return message;
}

// ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~
//     ObjectWrapper
// ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~


int ObjectWrapper::getInt(LPCWSTR property) {
  int result = 0;
  if (!d())
    return result;

  VARIANT vtProp;
  HRESULT hres = d()->Get(property, 0, &vtProp, 0, 0);
  if (SUCCEEDED(hres)) {
    result = vtProp.iVal;
    VariantClear(&vtProp);
  }
  return result;
}

std::wstring ObjectWrapper::getBSTR(LPCWSTR property) {
  std::wstring result = L"";
  if (!d())
    return result;

  VARIANT vtProp;
  HRESULT hres = d()->Get(property, 0, &vtProp, 0, 0);
  if (SUCCEEDED(hres)) {
    result = vtProp.bstrVal;
    VariantClear(&vtProp);
  }
  return result;
}

std::string ObjectWrapper::getCharArray(LPCWSTR property, const int MaxSize) {
  std::string result = "";
  if (!d())
    return result;

  VARIANT vtProp;
  HRESULT hres = d()->Get(property, 0, &vtProp, 0, 0);

  if (SUCCEEDED(hres) && vtProp.vt != VT_NULL && vtProp.parray != NULL) {
    char* buffer = (char*)malloc(MaxSize * sizeof(char));

    UINT32* serialArray = (UINT32*)vtProp.parray->pvData;
    for (int i = 0; i < MaxSize; ++i) {
      buffer[i] = (BYTE)(serialArray[i] & 0xff);
    }

    buffer[MaxSize - 1] = '\0';

    result = buffer;

    VariantClear(&vtProp);
    free(buffer);
  }

  return result;
}

std::string ObjectWrapper::getSizedCharArray(LPCWSTR property, LPCWSTR size_property) {
  const int array_len = getInt(L"UserFriendlyNameLength");
  return array_len > 0 ? getCharArray(L"UserFriendlyName", array_len) : "";
}

ObjectWrapper ObjectWrapper::getRef(LPCWSTR property, ServiceWrapper& sw) {
  std::wstring result = L"";
  if (!d())
    return ObjectWrapper(NULL);

  VARIANT vtProp;
  HRESULT hres = d()->Get(property, 0, &vtProp, 0, 0);
  if (SUCCEEDED(hres)) {
    auto result = sw.getObject(vtProp.bstrVal, hres);
    VariantClear(&vtProp);
    return result;
  }

  return ObjectWrapper(NULL);
}

void ObjectWrapper::scanWMIMonitorID(){
  bool any_show = false;
  const auto instanceName = getBSTR(L"InstanceName");
  if( !instanceName.empty() ) {
    any_show = true;
    std::wcout << instanceName.c_str();
  }

  const auto serialNumberID = getCharArray(L"SerialNumberID", 14);
  if( !serialNumberID.empty() ) {
    if( any_show )
      std::wcout << " - ";
    any_show = true;
    std::wcout << serialNumberID.c_str();
  }

  const auto userFriendlyName = getSizedCharArray(L"UserFriendlyName", L"UserFriendlyNameLength");
  if( !userFriendlyName.empty() ){
    if( any_show )
      std::wcout << " - ";
    any_show = true;
    std::wcout << userFriendlyName.c_str();
  }
  if (any_show)
    std::wcout << std::endl;
}

void ObjectWrapper::scanUSBHubName(){
  bool any_show = false;
  const auto deviceID = getBSTR(L"DeviceID");
  if( !deviceID.empty() ) {
    any_show = true;
    std::wcout << deviceID.c_str();
  }

  const auto deviceDesc = getBSTR(L"Description");
  if( !deviceDesc.empty() ) {
    if( any_show )
      std::wcout << " - ";
    any_show = true;
    std::wcout << deviceDesc.c_str();
  }

  if (any_show)
    std::wcout << std::endl;
}

// ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~
//     ObjectQueryWrapper
// ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~


ObjectWrapper ObjectQueryWrapper::Next() {
  if (!d())
    return ObjectWrapper(NULL);

  IWbemClassObject * result = NULL;
  ULONG uReturn = 0;

  HRESULT hr = d()->Next(WBEM_INFINITE, 1, &result, &uReturn);
  if (FAILED(hr) || !uReturn)
    return ObjectWrapper(NULL);

  return ObjectWrapper(result);
}




// ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~
//     ServiceWrapper
// ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~    ~~~~


ServiceWrapper::~ServiceWrapper() {
  if (pSvc)
    pSvc->Release();
  if (pLoc)
    pLoc->Release();
  CoUninitialize();
}

ServiceWrapper::ServiceWrapper(const OLECHAR* networkResource_p, HRESULT& hres) {
  ScopedBSTR networkResource(networkResource_p);

  hres = CoInitializeEx(0, COINIT_MULTITHREADED);
  if (FAILED(hres) && hres != RPC_E_CHANGED_MODE) 
    throw WMIH_Exception("Failed to initialize COM library.");

  hres = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
  if (FAILED(hres) && hres != RPC_E_TOO_LATE) 
    throw WMIH_Exception("Failed to initialize security.");

  hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *)&pLoc);
  if (FAILED(hres)) throw WMIH_Exception("Failed to create IWbemLocator object.");

  hres = pLoc->ConnectServer(_bstr_t(*networkResource), NULL, NULL, 0, NULL, 0, 0, &pSvc);
  if (FAILED(hres)) throw WMIH_Exception("Could not connect to WMI server.");

  hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
  if (FAILED(hres)) throw WMIH_Exception("Could not set proxy blanket.");
}
ObjectQueryWrapper ServiceWrapper::query(const OLECHAR* wql_p, const OLECHAR* select_p, HRESULT& hres) {
  ScopedBSTR wql(wql_p);
  ScopedBSTR select(select_p);

  return query(*wql,*select,hres);
}
ObjectQueryWrapper ServiceWrapper::query(const BSTR& wql, const BSTR& select, HRESULT& hres) {
  IEnumWbemClassObject* pEnumerator = NULL;
  hres = pSvc->ExecQuery(wql, select, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
  if (FAILED(hres)) throw WMIH_Exception("Query failed.");

  return ObjectQueryWrapper(pEnumerator);
}

ObjectWrapper ServiceWrapper::getObject(const OLECHAR* path_p, HRESULT& hres) {
  ScopedBSTR path(path_p);

  return getObject(*path,hres);
}

ObjectWrapper ServiceWrapper::getObject(const BSTR& path, HRESULT& hres) {
  IWbemClassObject * result = NULL;
  hres = pSvc->GetObject(path, NULL, NULL, &result, NULL);

  if (FAILED(hres))
    return ObjectWrapper(NULL);

  return ObjectWrapper(result);
}
