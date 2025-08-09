# Visual Leak Detector 

[![Build Status](https://github.com/avadae/vld/actions/workflows/msbuild.yml/badge.svg)](https://github.com/avadae/msbuild/actions)
[![GitHub Release](https://img.shields.io/github/v/release/avadae/vld?logo=github&sort=semver)](https://github.com/avadae/vld/releases/latest)

## This personal fork

I maintain this fork to be used by students at [Hogeschool West-Vlaanderen in the bachelor Digital Arts and Entertainment](https://www.digitalartsandentertainment.be/). It is created to keep the vld project up to date with the latest visual studio versions and the latest installer tools. We merge all changes applied to the original Azure fork into this one. Please refer to [https://github.com/Azure/vld](https://github.com/Azure/vld) for the original version.

## Documentation

Find the original documentation at [https://github.com/KindDragon/vld/wiki](https://github.com/KindDragon/vld/wiki) however it is outdated.

## Using VLD in Cmake projects

Starting version 2.5.11 you can easily use vld in cmake configured projects. Install VLD as usual. Then in your CMakeLists.txt you can write

```
find_package(VLD CONFIG)

target_include_directories(your_target PRIVATE ${VLD_INCLUDE_DIRS})
target_link_libraries(your_target PRIVATE ${VLD_LIBRARIES})
```

In your main.cpp (or other initial file for your exe/dll) add

```
#if _DEBUG
#if __has_include(<vld.h>)
#include <vld.h>
#endif
#endif
```

And you're good to go! If VLD is installed the project will be checked for leaks. If VLD is not installed the project still builds and leaks will be undetected.


## Updating dbghelp.dll

If you want to update the version of DbgHelp.dll in the project

- [Install the desired Windows SDK version.](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/)
- Copy dbghelp.dll from ```C:\Program Files (x86)\Windows Kits\10\Debuggers\x64``` to ```vld\setup\dbghelp\x64```
- Copy dbghelp.dll from ```C:\Program Files (x86)\Windows Kits\10\Debuggers\x86``` to ```vld\setup\dbghelp\x86```
- Update the version number in the manifest files in those folders
- Important: update the version number in the two dependency manifest files in ```vld\src```

