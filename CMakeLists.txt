# This is the 'CMakeLists.txt', pretty much I am following a template I already use (except for the MSVC part, I dislike MSVC).
# This will surely suffer a few changes in the next commits, but it is pretty much what it is.
# I will be adding specific notes down there, and in the other modules, of course.
# Also, I don't like CMake, I prefer Makefiles, but it is what it is.

cmake_minimum_required(VERSION 3.25)

# I am really creative, I know...
project(zclip CXX)

# So I was thinking of staying with C++20, it is modern enough and so on, but I actually like <print> a lot (I come from C, and 
# 'std::print'/'std::println' is close enough to 'printf', and better than 'std::cout'...). There are many other reasons, 
# but that one I like.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# We could debate in a performance sense whether or not we could allow extensions, but of course... We are working with MSVC, so...
# forget about it.
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (MSVC)
    add_compile_options(/W4 /WX /permissive- /utf-8)
    # I had to check this here: https://cmake.org/cmake/help/latest/variable/CMAKE_MSVC_RUNTIME_LIBRARY.html
    # "selects for all following targets a multi-threaded statically-linked runtime library with or without debug information depending on the configuration."
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else ()
    # I prefer gcc, or clang, also for testing in general, but in terms of portability, if MSVC handles it, you are usually fine.
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif ()

add_library(zclip_core STATIC
    src/clipboard_guard.cpp
    src/clipboard_listener.cpp
)

target_include_directories(zclip_core PUBLIC include)
target_link_libraries(zclip_core PUBLIC user32 kernel32)

add_executable(zclip src/main.cpp)
target_link_libraries(zclip PRIVATE zclip_core)

# This part is for code quality tooling...

file(GLOB_RECURSE ALL_SOURCE_FILES
    include/*.hpp
    src/*.cpp
)

find_program(CLANG_FORMAT_EXE NAMES clang-format)
if (CLANG_FORMAT_EXE)
    add_custom_target(format
        COMMAND ${CLANG_FORMAT_EXE} -i -style=file ${ALL_SOURCE_FILES}
        COMMENT "Applying deterministic clang-format style..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if (CLANG_TIDY_EXE)
    add_custom_target(tidy
        COMMAND ${CLANG_TIDY_EXE}
            -p ${CMAKE_BINARY_DIR}
            --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
            ${ALL_SOURCE_FILES}
        COMMENT "Running clang-tidy static code analysis..."
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()