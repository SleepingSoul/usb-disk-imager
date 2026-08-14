# ---------------------------------------------------------------------------
# Third-party dependencies.
#
# Everything except Qt is fetched at configure time, so a clean checkout builds with nothing but a
# compiler, CMake and Qt 6 installed. Distribution packagers can opt out per library with
# -DUDI_USE_SYSTEM_XXHASH=ON / -DUDI_USE_SYSTEM_SPDLOG=ON.
# ---------------------------------------------------------------------------

include(FetchContent)

set(UDI_XXHASH_VERSION "0.8.3" CACHE STRING "xxHash version to fetch")
set(UDI_SPDLOG_VERSION "1.15.1" CACHE STRING "spdlog version to fetch")

# --- xxHash ----------------------------------------------------------------
if (UDI_USE_SYSTEM_XXHASH)
    find_package(xxHash CONFIG QUIET)

    if (TARGET xxHash::xxhash)
        add_library(udi::xxhash ALIAS xxHash::xxhash)
        message(STATUS "xxHash: using the system package")
    else()
        find_package(PkgConfig QUIET)
        if (PkgConfig_FOUND)
            pkg_check_modules(SYSTEM_XXHASH IMPORTED_TARGET libxxhash)
        endif()

        if (TARGET PkgConfig::SYSTEM_XXHASH)
            add_library(udi::xxhash ALIAS PkgConfig::SYSTEM_XXHASH)
            message(STATUS "xxHash: using the system package through pkg-config")
        else()
            message(FATAL_ERROR
                "UDI_USE_SYSTEM_XXHASH=ON but no system xxHash was found. Install libxxhash-dev, or turn "
                "the option off to fetch it.")
        endif()
    endif()
else()
    # xxHash keeps its (unofficial) CMake project in a subdirectory, and its own minimum-required version
    # trips newer CMake releases. Pointing SOURCE_SUBDIR at a directory without a CMakeLists.txt makes
    # FetchContent download the sources without configuring them, so the single translation unit can be
    # compiled on our own terms.
    FetchContent_Declare(xxhash
        GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
        GIT_TAG v${UDI_XXHASH_VERSION}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        SOURCE_SUBDIR do-not-configure
    )
    FetchContent_MakeAvailable(xxhash)

    add_library(udi_xxhash STATIC "${xxhash_SOURCE_DIR}/xxhash.c")
    target_include_directories(udi_xxhash SYSTEM PUBLIC "${xxhash_SOURCE_DIR}")
    set_target_properties(udi_xxhash PROPERTIES
        C_STANDARD 99
        POSITION_INDEPENDENT_CODE ON
    )

    add_library(udi::xxhash ALIAS udi_xxhash)
    message(STATUS "xxHash: fetched v${UDI_XXHASH_VERSION}")
endif()

# --- spdlog ----------------------------------------------------------------
if (UDI_USE_SYSTEM_SPDLOG)
    find_package(spdlog CONFIG REQUIRED)
    add_library(udi::spdlog ALIAS spdlog::spdlog)
    message(STATUS "spdlog: using the system package")
else()
    FetchContent_Declare(spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v${UDI_SPDLOG_VERSION}
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )

    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SPDLOG_INSTALL OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(spdlog)

    add_library(udi::spdlog ALIAS spdlog)
    message(STATUS "spdlog: fetched v${UDI_SPDLOG_VERSION}")
endif()
