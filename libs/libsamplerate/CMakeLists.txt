option(LIBSAMPLERATE_COMPATIBLE_NAME "Use old dll name on Windows platform" OFF)

option(LIBSAMPLERATE_ENABLE_SINC_FAST_CONVERTER "Enable Fastest Sinc Interpolator converter" ON)
option(LIBSAMPLERATE_ENABLE_SINC_MEDIUM_CONVERTER "Enable Medium Sinc Interpolator converter" ON)
option(LIBSAMPLERATE_ENABLE_SINC_BEST_CONVERTER "Enable Best Sinc Interpolator converter" ON)

option(LIBSAMPLERATE_INSTALL_PKGCONFIG_MODULE "Install samplerate.pc PkgConfig module." ON)

set(ENABLE_SINC_FAST_CONVERTER ${LIBSAMPLERATE_ENABLE_SINC_FAST_CONVERTER} PARENT_SCOPE)
set(ENABLE_SINC_MEDIUM_CONVERTER ${LIBSAMPLERATE_ENABLE_SINC_MEDIUM_CONVERTER} PARENT_SCOPE)
set(ENABLE_SINC_BEST_CONVERTER ${LIBSAMPLERATE_ENABLE_SINC_BEST_CONVERTER} PARENT_SCOPE)

include(CMakePushCheckState)
include(CheckCSourceCompiles)
include(ClipMode)
include(CMakePackageConfigHelpers)

# This will set CPU_CLIPS_NEGATIVE and CPU_CLIPS_POSITIVE
clip_mode()

# check for symbol visibility attributes support
cmake_push_check_state()
set(CMAKE_REQUIRED_FLAGS "-fvisibility=hidden -Werror")
check_c_source_compiles("__attribute__((visibility(\"default\"))) int foo(void);
                         __attribute__((visibility(\"hidden\")))  int bar(void);
                         int foo (void) {return 0;}
                         int bar (void) {return 1;}
                         int main(void) {return 0;}" HAVE_VISIBILITY)
cmake_pop_check_state()

add_library(samplerate
  common.h
  fastest_coeffs.h
  high_qual_coeffs.h
  mid_qual_coeffs.h
  samplerate.c
  samplerate.h
  src_linear.c
  src_sinc.c
  src_zoh.c
  $<$<AND:$<BOOL:${WIN32}>,$<BOOL:${BUILD_SHARED_LIBS}>>:../Win32/libsamplerate-0.def>)

target_include_directories(samplerate
        INTERFACE "."
        PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../")

# ALIAS to use if libsamplerate is included from other project with add_subdirectory()
add_library(SampleRate::samplerate ALIAS samplerate)

# CMake generates wrong DLL names for MinGW and Cygwin, fix it
if(BUILD_SHARED_LIBS AND WIN32)
  if(LIBSAMPLERATE_COMPATIBLE_NAME)
    if(MSVC)
      set_target_properties(samplerate PROPERTIES OUTPUT_NAME "libsamplerate-${libsamplerate_VERSION_MAJOR}")
    else()
      set_target_properties(samplerate PROPERTIES OUTPUT_NAME "samplerate-${libsamplerate_VERSION_MAJOR}")
    endif()
  else()
    if(MINGW OR CYGWIN)
      set_target_properties(samplerate PROPERTIES RUNTIME_OUTPUT_NAME "samplerate-${libsamplerate_VERSION_MAJOR}")
    endif()
  endif()
  if(OS2)
    # OS/2 doesn't support a DLL name longer than 8 characters.
    set_target_properties(samplerate PROPERTIES OUTPUT_NAME "SAMPRATE")
    set_target_properties(samplerate PROPERTIES PREFIX "")
  endif()
endif()

target_include_directories(samplerate
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

if(LIBM_REQUIRED)
    target_link_libraries(samplerate PRIVATE m)
endif()

# Set public header
set_property(TARGET samplerate PROPERTY PUBLIC_HEADER ${PROJECT_SOURCE_DIR}/include/samplerate.h)

if(BUILD_SHARED_LIBS)
  # Set ABI version. This is critical for Unix-like OSes
  set_target_properties(samplerate PROPERTIES
    VERSION ${libsamplerate_VERSION}
    SOVERSION ${libsamplerate_VERSION_MAJOR})

    # Use Version_script to export ABI set
    if(UNIX AND (NOT APPLE))
      if((CMAKE_C_COMPILER_ID STREQUAL "GNU") OR
         (CMAKE_C_COMPILER_ID STREQUAL "Clang") OR
         (CMAKE_C_COMPILER_ID STREQUAL "Intel"))

        if(CMAKE_VERSION VERSION_LESS 3.13)
          # This works 
          set_property(TARGET samplerate APPEND_STRING PROPERTY
            LINK_FLAGS "-Wl,--version-script,${CMAKE_CURRENT_BINARY_DIR}/Version_script")
        else()
          # This works even better, e.g. for Clang it uses `-Xlinker` option,
          # but requires CMake >= 3.13.
          target_link_options(samplerate
            PRIVATE
              "LINKER:--version-script,${CMAKE_CURRENT_BINARY_DIR}/Version_script")
        endif()

      endif()
    endif()

  # keep in sync with configure.ac
  set (LIBSAMPLERATE_ABI_VERSION_CURRENT 2)
  set (LIBSAMPLERATE_ABI_VERSION_REVISION 2)
  set (LIBSAMPLERATE_ABI_VERSION_AGE 2)
  math (EXPR LIBSAMPLERATE_MACHO_COMPATIBILITY_VERSION "${LIBSAMPLERATE_ABI_VERSION_CURRENT} + 1")
  set (LIBSAMPLERATE_MACHO_CURRENT_VERSION "${LIBSAMPLERATE_MACHO_COMPATIBILITY_VERSION}.${LIBSAMPLERATE_ABI_VERSION_REVISION}.0")
  if (APPLE)
    if (NOT (CMAKE_VERSION VERSION_LESS 3.17))
      set_target_properties (samplerate PROPERTIES
        MACHO_CURRENT_VERSION ${LIBSAMPLERATE_MACHO_CURRENT_VERSION}
        MACHO_COMPATIBILITY_VERSION ${LIBSAMPLERATE_MACHO_COMPATIBILITY_VERSION}
        )
    else ()
      message (FATAL_ERROR "Apple platform requires cmake >= 3.17 to build dylib.")
    endif ()
  endif ()
endif()

# Istallation

if(LIBSAMPLERATE_INSTALL)

  install(TARGETS samplerate EXPORT SampleRateTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

  # pkg-config module

  if(LIBSAMPLERATE_INSTALL_PKGCONFIG_MODULE)
    set(prefix ${CMAKE_INSTALL_PREFIX})
    set(exec_prefix "\${prefix}")
    set(includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
    set(libdir "\${exec_prefix}/${CMAKE_INSTALL_LIBDIR}")
    set(VERSION "${PROJECT_VERSION}")
    if(LIBM_REQUIRED)
      set(LIBS "-lm")
    endif()
    configure_file(../samplerate.pc.in samplerate.pc @ONLY)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/samplerate.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
  endif()

  set(CMAKE_INSTALL_PACKAGEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/SampleRate")

  configure_package_config_file(../cmake/SampleRateConfig.cmake.in SampleRateConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_PACKAGEDIR})

  write_basic_package_version_file(SampleRateConfigVersion.cmake COMPATIBILITY SameMajorVersion)
  install(
    FILES
      ${CMAKE_CURRENT_BINARY_DIR}/SampleRateConfig.cmake
      ${CMAKE_CURRENT_BINARY_DIR}/SampleRateConfigVersion.cmake
    DESTINATION
      ${CMAKE_INSTALL_PACKAGEDIR})

  install(EXPORT SampleRateTargets
    NAMESPACE SampleRate::
    DESTINATION ${CMAKE_INSTALL_PACKAGEDIR})

endif()
