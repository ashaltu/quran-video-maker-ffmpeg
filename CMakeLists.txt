cmake_minimum_required(VERSION 3.16)
project(QuranVideoMakerFFmpeg CXX)
set(CPACK_PACKAGE_NAME "qvm")
set(CPACK_PACKAGE_VERSION "0.0.0-dev" CACHE STRING "Package version")
set(CPACK_PACKAGE_VENDOR "ashaltu")
set(CPACK_GENERATOR "ZIP")
if(WIN32)
    # Keep Windows artifacts named with a -win64 suffix for Scoop and release automation.
    set(CPACK_SYSTEM_NAME "win64")
elseif(APPLE)
    set(CPACK_SYSTEM_NAME "macos")
else()
    set(CPACK_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}")
endif()
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CPACK_SYSTEM_NAME}")

include(InstallRequiredSystemLibraries)
include(CPack)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Dependency: cxxopts
find_package(cxxopts CONFIG)
if(NOT cxxopts_FOUND)
    FetchContent_Declare(
      cxxopts
      GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
      GIT_TAG        v3.2.0
    )
    FetchContent_MakeAvailable(cxxopts)
endif()

# Dependency: cpr
find_package(cpr CONFIG)
if(WIN32)
    # Always build CPR/cURL ourselves on Windows so we can force the Schannel
    # backend and rely on the native certificate store instead of a missing
    # OpenSSL CA bundle in Scoop installs.
    set(cpr_FOUND FALSE)
endif()
if(NOT cpr_FOUND)
    set(CPR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(CPR_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(CPR_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(CPR_FORCE_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)
    if(WIN32)
        set(CPR_CURL_SSL_BACKEND "Schannel" CACHE STRING "" FORCE)
        set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
        set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
    endif()
    FetchContent_Declare(
      cpr
      GIT_REPOSITORY https://github.com/libcpr/cpr.git
      GIT_TAG        1.11.3
    )
    FetchContent_MakeAvailable(cpr)
endif()

# Dependency: nlohmann_json
find_package(nlohmann_json CONFIG)
if(NOT nlohmann_json_FOUND)
    FetchContent_Declare(
      nlohmann_json
      GIT_REPOSITORY https://github.com/nlohmann/json.git
      GIT_TAG        v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Dependency: googletest
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

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


add_library(qvm_lib STATIC
    src/LiveApiClient.cpp src/LiveApiClient.h
    src/SystemProcessExecutor.cpp src/SystemProcessExecutor.h
    src/interfaces/IApiClient.h
    src/interfaces/IProcessExecutor.h
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

add_executable(qvm src/main.cpp)

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

target_link_libraries(qvm PRIVATE qvm_lib)

add_executable(unit_tests tests/unit_tests.cpp)
target_link_libraries(unit_tests PRIVATE qvm_lib)

add_executable(reciter_listing_test tests/test_reciter_listing.cpp)
target_link_libraries(reciter_listing_test PRIVATE qvm_lib gtest gmock gtest_main)


enable_testing()
add_test(NAME unit COMMAND unit_tests WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
add_test(NAME reciter_listing COMMAND reciter_listing_test WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
    target_link_libraries(qvm PRIVATE stdc++fs)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.0)
    target_link_libraries(qvm PRIVATE stdc++fs)
endif()

# Installation rules
include(GNUInstallDirs)

install(TARGETS qvm
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES config.json
    DESTINATION ${CMAKE_INSTALL_DATADIR}/quran-video-maker
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data.tar")
    install(FILES data.tar
        DESTINATION ${CMAKE_INSTALL_DATADIR}/quran-video-maker
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/data")
    install(DIRECTORY data
        DESTINATION ${CMAKE_INSTALL_DATADIR}/quran-video-maker
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets")
    install(DIRECTORY assets
        DESTINATION ${CMAKE_INSTALL_DATADIR}/quran-video-maker
    )
endif()

if(WIN32)
    # Optional: include a CA bundle alongside the binary for OpenSSL-based curls
    if(EXISTS "${CMAKE_BINARY_DIR}/ca-bundle.crt")
        install(FILES "${CMAKE_BINARY_DIR}/ca-bundle.crt"
            DESTINATION etc/ssl/certs
            RENAME ca-bundle.crt
        )
    endif()
endif()

if(WIN32 AND MINGW)
    install(DIRECTORY ${CMAKE_BINARY_DIR}/deps/
        DESTINATION ${CMAKE_INSTALL_BINDIR}
        FILES_MATCHING PATTERN "*.dll"
    )
endif()
