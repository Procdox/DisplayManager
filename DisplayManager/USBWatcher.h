#pragma once

#include "common.h"
#include <vector>

using hubList = std::vector<std::pair<std::wstring, std::wstring>>;


//bool isHubConnected(const std::wstring& queryID);
//hubList getConnectedHubs();

bool isUSBConnected(const std::wstring& queryID);
hubList getConnectedUSB();
