# Device Switcher plugin for OBS Studio

Plugin for OBS Studio to add a device switcher dock.

# Build
1. In-tree build
    - Build OBS Studio: https://obsproject.com/wiki/Install-Instructions
    - Check out this repository to UI/frontend-plugins/device-switcher
    - Add `add_subdirectory(device-switcher)` to UI/frontend-plugins/CMakeLists.txt
    - Rebuild OBS Studio

1. Stand-alone build (Linux only)
    - Verify that you have package with development files for OBS
    - Check out this repository and run `cmake -S . -B build -DBUILD_OUT_OF_TREE=On && cmake --build build`

# Donations
https://www.paypal.me/exeldro

Thanks to the contributions of [EF Education First](https://ef.com)
