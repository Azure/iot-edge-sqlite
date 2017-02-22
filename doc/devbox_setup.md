# Prepare your development environment

This document describes how to prepare your development environment to use the *Microsoft Azure IoT Gateway SQLite Module*. It requires a pre-setup environment for the *Microsoft Azure IoT Gateway SDK*.

- [Setting up an environment for Microsoft Azure IoT Gateway SDK](https://github.com/Azure/azure-iot-gateway-sdk/blob/master/doc/devbox_setup.md)

# Compile the SQLite module

This section shows you how to integrate the SQLite module with the Azure IoT Gateway SDK and compile it.

## Prepare SQLite3 library

Vist [Download Page](https://www.sqlite.org/download.html) of SQLite Official Website.

### Windows
1. Download from *Precompiled Binaries for Windows*.
2. Extract all files, copy **sqlite3.h** to the `modules\\sqlite\\inc` folder of the Azure IoT Gateway SQLite.
3. Open a Developer Command Prompt for VS2015 as an Administrator, navigate to the directory where **sqlite3.def** is stored.
4. Run `lib /def:sqlite3.def /out:sqlite3.lib`.
5. Edit the **CMakeLists.txt** in the `modules\\sqlite` directory, replace `/sqlite3_library/path` and `/sqlite3_dll/path` with the path where **sqlite3.lib** and **sqlite3.dll** locate.

### Linux
1. Download from *Source Code*.
2. Extract all files.
3. Open a shell, navigate to the source code directory.
4. Run `./configure;make;sudo make install;`.
5. Edit the **CMakeLists.txt** in the `modules/sqlite` directory, replace `/sqlite3_library/path` with the path where **libsqlite3.so** is installed.

## Build the SQLite Module
1. Copy the `modules\\sqlite` folder from the Azure IoT Gateway SQLite and paste it into the **modules** directory of the Azure IoT Gateway SDK.
2. Copy the `samples\\sqlite_sample` folder from the Azure IoT Gateway SQLite and paste it into the **samples** directory of the Azure IoT Gateway SDK.
3. Edit the **CMakeLists.txt** in the **modules** directory. Add this line `add_subdirectory(sqlite)` to the end of the file.
4. Edit the **CMakeLists.txt** in the **samples** directory. Add this line `add_subdirectory(sqlite_sample)` to the end of the file.
5. [Compile the Azure IoT Gateway SDK](https://github.com/Azure/azure-iot-gateway-sdk/blob/master/samples/hello_world/README.md#how-to-build-the-sample) for your machine.
