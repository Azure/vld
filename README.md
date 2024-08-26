# Visual Leak Detector 

## This fork

This fork is created to keep the vld project up to date with the latest visual studio versions and the latest installer tools. We merge all changes applied to the original Azure fork into this one.

## Microsoft Fork

This fork was created to address some of the issues found in the original repo [https://github.com/KindDragon/vld](https://github.com/KindDragon/vld), which has not been updated since November 24, 2017. The changes in this repo can be merged back into the original repo if it is again accepting pull requests.

If you would like to contribute to this fork, please submit a pull request. It will be looked at on a "best effort" basis as our team is available.

## Introduction

Visual C++ provides built-in memory leak detection, but its capabilities are minimal at best. This memory leak detector was created as a free alternative to the built-in memory leak detector provided with Visual C++. Here are some of Visual Leak Detector's features, none of which exist in the built-in detector:

*   Provides a complete stack trace for each leaked block, including source file and line number information when available.
*   Detects most, if not all, types of in-process memory leaks including COM-based leaks, and pure Win32 heap-based leaks.
*   Selected modules (DLLs or even the main EXE) can be excluded from leak detection.
*   Provides complete data dumps (in hex and ASCII) of leaked blocks.
*   Customizable memory leak report: can be saved to a file or sent to the debugger and can include a variable level of detail.

Other after-market leak detectors for Visual C++ are already available. But most of the really popular ones, like Purify and BoundsChecker, are very expensive. A few free alternatives exist, but they're often too intrusive, restrictive, or unreliable. Visual Leak Detector is currently the only freely available memory leak detector for Visual C++ that provides all of the above professional-level features packaged neatly in an easy-to-use library.

## Documentation

Read the documentation at [https://github.com/KindDragon/vld/wiki](https://github.com/KindDragon/vld/wiki)
