set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

cmake_minimum_required(VERSION 3.7.2)
project("Broom" VERSION 1.0.0 LANGUAGES C)
set(CMAKE_C_STANDARD 23)

# Autotools variables
set(top_srcdir ${CMAKE_CURRENT_SOURCE_DIR})

# AC_INIT variables
set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_TARNAME "broom")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(PACKAGE_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")

set(PACKAGE_COPYRIGHT "Copyright (C) 1993-2025")
set(PACKAGE_LICENSE "GNU General Public License, version 3")

# Any settings that should apply to all targets in this directory and all
# subdirectories should go here.  Use judiciously.
if(MSVC)
    add_definitions("/D_CRT_SECURE_NO_WARNINGS" "/D_CRT_SECURE_NO_DEPRECATE" "/D_CRT_NONSTDC_NO_DEPRECATE")
else()
    add_compile_options("-Wall" "-Wredundant-decls")
endif()

option(CMAKE_FIND_PACKAGE_PREFER_CONFIG "Lookup package config files before using find modules" On)

option(ENABLE_SDL2_NET "Enable SDL2_net" On)
option(ENABLE_SDL2_MIXER "Enable SDL2_mixer" On)

# Enable libsamplerate with all conversion options
set(HAVE_LIBSAMPLERATE FALSE)
add_definitions(
        -DENABLE_SINC_FAST_CONVERTER
        -DENABLE_SINC_BEST_CONVERTER
        -DENABLE_SINC_MEDIUM_CONVERTER
)

find_package(SDL2 2.0.14)
if(ENABLE_SDL2_MIXER)
    find_package(SDL2_mixer 2.0.2)
else()
    add_compile_definitions(DISABLE_SDL2MIXER=1)
endif()

if(ENABLE_SDL2_NET)
    find_package(SDL2_net 2.0.0)
else()
    add_compile_definitions(DISABLE_SDL2NET=1)
endif()

# Check for libpng.
find_package(PNG)
if(PNG_FOUND)
    set(HAVE_LIBPNG TRUE)
endif()

# Check for FluidSynth.
find_package(FluidSynth 2.2.0)
if(FluidSynth_FOUND AND ENABLE_SDL2_MIXER)
    set(HAVE_FLUIDSYNTH TRUE)
endif()

find_package(m)

include(CheckSymbolExists)
include(CheckIncludeFile)
check_symbol_exists(strcasecmp "strings.h" HAVE_DECL_STRCASECMP)
check_symbol_exists(strncasecmp "strings.h" HAVE_DECL_STRNCASECMP)
check_include_file("dirent.h" HAVE_DIRENT_H)

string(CONCAT WINDOWS_RC_VERSION "${PROJECT_VERSION_MAJOR}, " "${PROJECT_VERSION_MINOR}, ${PROJECT_VERSION_PATCH}, 0")

configure_file(cmake/config.h.cin config.h)
configure_file(src/doom-res.rc.in src/doom-res.rc)

add_subdirectory("libs")
add_subdirectory("src")
