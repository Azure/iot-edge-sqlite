#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.11)

set(sqlite_sources
    ./src/sqlite.c
)

set(sqlite_headers
    ./inc/sqlite.h
)


include_directories(./inc)
include_directories(${GW_INC})

#this prepares the sqlite3 library
find_library(SQLITE3_LIB NAMES sqlite3 PATHS /sqlite3_library/path)
find_file(SQLITE3_DLL NAMES sqlite3.dll PATHS /sqlite3_dll/path)

if(SQLITE3_LIB)

    add_library(sqlite3 SHARED IMPORTED)
if(WIN32)
    set_target_properties(sqlite3 PROPERTIES IMPORTED_LOCATION ${SQLITE3_DLL})
    set_target_properties(sqlite3 PROPERTIES IMPORTED_IMPLIB ${SQLITE3_LIB})
else(WIN32)
    set_target_properties(sqlite3 PROPERTIES IMPORTED_LOCATION ${SQLITE3_LIB})
endif(WIN32)

endif(SQLITE3_LIB)

#this builds the sqlite dynamic library
add_library(sqlite MODULE ${sqlite_sources}  ${sqlite_headers})
target_link_libraries(sqlite gateway sqlite3)

#this builds the sqlite static library
add_library(sqlite_static  ${sqlite_sources} ${sqlite_headers})
target_compile_definitions(sqlite_static PRIVATE BUILD_MODULE_TYPE_STATIC)
target_link_libraries(sqlite_static gateway sqlite3)

linkSharedUtil(sqlite)
linkSharedUtil(sqlite_static)

add_module_to_solution(sqlite)
if(${run_unittests})
	add_subdirectory(tests)
endif()

if(install_modules)
    install(TARGETS sqlite LIBRARY DESTINATION "${LIB_INSTALL_DIR}/modules") 
endif()

