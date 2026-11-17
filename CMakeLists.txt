cmake_minimum_required(VERSION 3.30)
project(stumbling_bot_v2 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(warnings_interface INTERFACE)

target_compile_options(warnings_interface INTERFACE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /fp:strict>
)

add_executable(stumbling_bot_v2
    src/main.cpp
    src/app.hpp
    src/app.cpp
    src/search.hpp
    src/search.cpp
    src/eval.hpp
    src/eval.cpp
    src/perft.hpp
    src/perft.cpp
    src/board.hpp
    src/board.cpp
    src/utils.hpp
    src/utils.cpp
)

if (MSVC)
    target_compile_options(stumbling_bot_v2 PRIVATE
        $<$<CONFIG:RelWithDebInfo>:/O2>
        $<$<CONFIG:RelWithDebInfo>:/Ob2>
        $<$<CONFIG:RelWithDebInfo>:/Zo>
        $<$<CONFIG:RelWithDebInfo>:/arch:AVX2>
    )

    target_link_options(stumbling_bot_v2 PRIVATE
        $<$<CONFIG:RelWithDebInfo>:/LTCG>
    )
endif()

set_property(TARGET stumbling_bot_v2 PROPERTY CXX_STANDARD 23)

target_link_libraries(stumbling_bot_v2 PRIVATE warnings_interface)