cmake_minimum_required(VERSION 3.15)
project(emtypyie-cli-c VERSION 3.0.1 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -Wno-missing-field-initializers")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

add_executable(emtypyie
    src/main.c
    src/shell.c
    src/theme.c
    src/util.c
    src/fetch.c
    src/download.c
    src/project.c
    src/bakafetch.c
    src/runtime.c
    src/auth.c
    src/effects.c
    src/larpino.cpp
    lib/cJSON.c
)

target_include_directories(emtypyie PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/lib
)

if(WIN32)
    target_link_libraries(emtypyie PRIVATE winhttp)
else()
    find_package(CURL REQUIRED)
    target_link_libraries(emtypyie PRIVATE CURL::libcurl)
endif()
