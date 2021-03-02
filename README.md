# DisplayManager

DisplayManager is a tool for managing and automatically switching between display input profiles across multiple monitors. It is designed to work with external USB switches, allowing a single button to switch multiple USB devices and monitors between two PCs, without the need for a costly modern KVM switch.

## Installation
Built with:
- Windows 10 Pro
- MSVC 2017
- QT 5.12.3

Steps:
- Compile a release build
- Copy the produced .exe and the following .ddl's from the QT installation to the desired location
  - Qt5Core.dll
  - Qt5Gui.dll
  - Qt5Qml.dll
  - Qt5Quick.dll
  - Qt5Widgets.dll


## Usage

Currently connected devices are listed in the left list. Selecting one will list the available display inputs in the right pane, as well as allow you to nickname the monitor for ease of identification.
The right list will highlight the current input. Selecting an input will change the monitor to that input. 

If you change the monitors connected to the computer while the software is running, the Device list can be refreshed with the "Refresh Devices" button.

Profiles can be saved and toggled between with the menu buttons, or the following keyboard shortcuts:
- Toggle Profile: Numpad * + Numpad 8
- Save Local Profile: Numpad * + Numpad 1
- Save Alt Profile: Numpad * + Numpad 2

The manager can automatically switch between the local and alt profile depending on if a specific USB device is connected. "Select HUB" will allow you to choose which device should be monitored. "watch HUB" will enabled this behavior if a hub is selected and currently connected. When the device is connected to the PC running this software, the local profile will be switched to. If the device is not connected, the alt profile will be switched to.