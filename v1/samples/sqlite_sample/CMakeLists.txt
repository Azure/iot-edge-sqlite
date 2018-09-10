#Copyright (c) Microsoft. All rights reserved.
#Licensed under the MIT license. See LICENSE file in the project root for full license information.

cmake_minimum_required(VERSION 2.8.12)

set(sqlite_sources
    ./src/main.c
)
if(WIN32)
    set(sqlite_sources
        ${sqlite_sources}
        ./src/sqlite_win.json
    )
    set_source_files_properties(./src/sqlite_win.json PROPERTIES HEADER_FILE_ONLY ON)
else()
    set(sqlite_sources 
        ${sqlite_sources}
        ./src/sqlite_lin.json
    )
    set_source_files_properties(./src/sqlite_lin.json PROPERTIES HEADER_FILE_ONLY ON)
endif()

include_directories(./inc ${IOTHUB_CLIENT_INC_FOLDER})
include_directories(${GW_INC})
include_directories(../../modules/common)

add_executable(sqlite_sample ${sqlite_sources})

add_dependencies(sqlite_sample sqlite identity_map iothub modbus_read)

target_link_libraries(sqlite_sample gateway)
linkSharedUtil(sqlite_sample)
install_broker(sqlite_sample ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration) )
copy_gateway_dll(sqlite_sample ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration) )
copy_iothub_client_dll(sqlite_sample ${CMAKE_CURRENT_BINARY_DIR}/$(Configuration) )

add_sample_to_solution(sqlite_sample)