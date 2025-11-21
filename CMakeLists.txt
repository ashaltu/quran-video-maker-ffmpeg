cmake_minimum_required(VERSION 3.16)
project(QuranVideoMakerFFmpeg CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Prevent FetchContent dependencies from being installed
set(CXXOPTS_BUILD_EXAMPLES OFF CACHE BOOL "")
set(CXXOPTS_BUILD_TESTS OFF CACHE BOOL "")
set(CXXOPTS_ENABLE_INSTALL OFF CACHE BOOL "")

set(CPR_ENABLE_INSTALL OFF CACHE BOOL "")
set(CPR_USE_SYSTEM_CURL ON CACHE BOOL "")  # Use system curl instead of building it
set(BUILD_SHARED_LIBS OFF CACHE BOOL "")  # Build all libraries as static for easier distribution

set(JSON_BuildTests OFF CACHE BOOL "")
set(JSON_Install OFF CACHE BOOL "")

FetchContent_Declare(
  cxxopts
  GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
  GIT_TAG        v3.2.0
)
FetchContent_MakeAvailable(cxxopts)

FetchContent_Declare(
  cpr
  GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG        1.10.5
)
FetchContent_MakeAvailable(cpr)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# Find required system libraries
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

# Find FFmpeg libraries using PkgConfig. Request IMPORTED_TARGET so link and
# include directories are propagated correctly on all platforms (e.g. macOS).
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVFILTER REQUIRED IMPORTED_TARGET libavfilter)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWSCALE REQUIRED IMPORTED_TARGET libswscale)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(HARFBUZZ REQUIRED IMPORTED_TARGET harfbuzz)


# Use static library for easier distribution
add_library(qvm_lib STATIC
    src/api.cpp src/api.h
    src/video_generator.cpp src/video_generator.h
    src/timing_parser.cpp src/timing_parser.h
    src/config_loader.cpp src/config_loader.h
    src/metadata_writer.cpp src/metadata_writer.h
    src/cache_utils.cpp src/cache_utils.h
    src/recitation_utils.cpp src/recitation_utils.h
    src/subtitle_builder.cpp src/subtitle_builder.h
    src/localization_utils.cpp src/localization_utils.h
    src/audio/custom_audio_processor.cpp src/audio/custom_audio_processor.h
    src/text/text_layout.cpp src/text/text_layout.h
    src/types.h
)

add_executable(quran-video-maker src/main.cpp)

target_include_directories(qvm_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(qvm_lib PUBLIC
    PkgConfig::AVFORMAT
    PkgConfig::AVCODEC
    PkgConfig::AVFILTER
    PkgConfig::AVUTIL
    PkgConfig::SWSCALE
    PkgConfig::FREETYPE
    PkgConfig::HARFBUZZ
    cpr::cpr
    nlohmann_json::nlohmann_json
    Threads::Threads
    cxxopts::cxxopts
)

target_link_libraries(quran-video-maker PRIVATE qvm_lib)

add_executable(unit_tests tests/unit_tests.cpp)
target_link_libraries(unit_tests PRIVATE qvm_lib)

enable_testing()
add_test(NAME unit COMMAND unit_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
    target_link_libraries(quran-video-maker PRIVATE stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
    target_link_libraries(quran-video-maker PRIVATE stdc++fs)
endif()

# Installation rules
include(GNUInstallDirs)

# Install the main executable
install(TARGETS quran-video-maker
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# Install default configuration file
install(FILES config.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/qvm-ffmpeg)

# Install assets directory (fonts, videos, etc.)
install(DIRECTORY assets/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/qvm-ffmpeg/assets)

# Install data directory (if present - typically extracted from data.tar)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    install(DIRECTORY data/
        DESTINATION ${CMAKE_INSTALL_DATADIR}/qvm-ffmpeg/data)
endif()
