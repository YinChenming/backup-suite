if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(VCPKG_TARGET_TRIPLET "x64-windows-dynamic" CACHE STRING "VCPKG triplet for MSVC")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "VCPKG triplet for MinGW")
endif ()

find_package(unofficial-sqlite3 REQUIRED)
find_package(LibArchive QUIET)

set(CORE_SOURCES
        src/filesystem/system_device.cpp
        src/utils/time_converter.cpp
        src/utils/tar.cpp
        src/utils/admin_privilege.cpp
        src/filesystem/device.cpp
        src/backup/backup_controller.cpp
        src/utils/database.cpp
        src/utils/sevenzip_backend.cpp
        src/utils/crc.cpp
        src/utils/zip.cpp
        src/filesystem/entities.cpp
        src/utils/tmpfile.cpp
        src/filesystem/compresses_device.cpp
        src/filesystem/seven_zip_device.cpp
        src/encryption/zip_crypto.cpp
        src/encryption/rc.cpp
        #        src/compress/deflate.cpp
)

# 将include目录添加到项目中
add_library(backup_suite_core SHARED
        ${CORE_SOURCES}
)

target_compile_definitions(backup_suite_core PRIVATE BACKUP_SUITE_CORE_EXPORTS)

target_include_directories(backup_suite_core
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(backup_suite_core PUBLIC unofficial::sqlite3::sqlite3)
if (TARGET LibArchive::LibArchive)
    target_link_libraries(backup_suite_core PUBLIC LibArchive::LibArchive)
else ()
    message(WARNING "LibArchive::LibArchive not found; core will build without linking to libarchive")
endif ()

# Optional 7z support
option(BACKUPSUITE_ENABLE_7Z "Enable 7-Zip SDK based 7z device" ON)
if (BACKUPSUITE_ENABLE_7Z)
    target_compile_definitions(backup_suite_core PUBLIC BACKUPSUITE_ENABLE_7Z=1)
    # 固定 vendor 源码路径：<repo>/third_party/p7zip
    if (NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party")
        file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party")
    endif ()
    set(BACKUPSUITE_P7ZIP_SOURCE_DIR "${CMAKE_SOURCE_DIR}/third_party/p7zip" CACHE PATH "Vendor p7zip source directory")
    if (NOT EXISTS "${BACKUPSUITE_P7ZIP_SOURCE_DIR}")
        message(STATUS "p7zip not found at ${BACKUPSUITE_P7ZIP_SOURCE_DIR}, attempting to clone...")
        find_program(GIT_EXECUTABLE git)
        if (GIT_EXECUTABLE)
            set(BACKUPSUITE_7Z_REPO "https://github.com/p7zip-project/p7zip" CACHE STRING "Git repository for p7zip sources")
            set(BACKUPSUITE_7Z_TAG "v17.06" CACHE STRING "Git tag/commit for p7zip sources")
            execute_process(
                    COMMAND "${GIT_EXECUTABLE}" clone --depth 1 --branch ${BACKUPSUITE_7Z_TAG} ${BACKUPSUITE_7Z_REPO} "${BACKUPSUITE_P7ZIP_SOURCE_DIR}"
                    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party"
                    RESULT_VARIABLE _clone_res
                    # OUTPUT_QUIET ERROR_QUIET
            )
            if (NOT _clone_res EQUAL 0)
                message(WARNING "Failed to clone p7zip. Please manually download to ${BACKUPSUITE_P7ZIP_SOURCE_DIR} and re-configure.")
                message(WARNING "You can use: git clone --depth 1 --branch ${BACKUPSUITE_7Z_TAG} ${BACKUPSUITE_7Z_REPO} ${BACKUPSUITE_P7ZIP_SOURCE_DIR}")
            else ()
                message(STATUS "Cloned p7zip into ${BACKUPSUITE_P7ZIP_SOURCE_DIR}")
            endif ()
        else ()
            message(WARNING "Git not found; please manually place p7zip at ${BACKUPSUITE_P7ZIP_SOURCE_DIR}.")
        endif ()
    else ()
        message(STATUS "Using existing p7zip at: ${BACKUPSUITE_P7ZIP_SOURCE_DIR}")
    endif ()

    # OpenSSL can be used for AES when integrating 7z encryption paths
    find_package(OpenSSL QUIET)
    if (OpenSSL_FOUND)
        target_link_libraries(backup_suite_core PUBLIC OpenSSL::SSL OpenSSL::Crypto)
    else ()
        message(STATUS "OpenSSL not found; AES integration for SevenZipDevice will be disabled until provided.")
    endif ()

    option(BACKUPSUITE_USE_PREBUILT_7Z_DLL "Use prebuilt 7z*.dll instead of building p7zip sources" ON)
    set(PREBUILT_7Z_DLL "${CMAKE_SOURCE_DIR}/third_party/7-Zip/7z.dll" CACHE FILEPATH "Path to prebuilt 7z.dll")
    set(PREBUILT_7Z_DLL_ALT "${CMAKE_SOURCE_DIR}/third_party/7zip/bin/7z.dll" CACHE FILEPATH "Alternative path to prebuilt 7z.dll")
    set(PREBUILT_7ZIP_DLL "${CMAKE_SOURCE_DIR}/third_party/7-Zip/7-zip.dll" CACHE FILEPATH "Path to prebuilt 7-zip.dll")
    set(PREBUILT_7Z_EXE "${CMAKE_SOURCE_DIR}/third_party/7-Zip/7z.exe" CACHE FILEPATH "Path to prebuilt 7z.exe")
    set(PREBUILT_7Z_EXE_ALT "${CMAKE_SOURCE_DIR}/third_party/7zip/bin/7z.exe" CACHE FILEPATH "Alternative path to prebuilt 7z.exe")
    set(PREBUILT_7ZA_EXE "${CMAKE_SOURCE_DIR}/third_party/7-Zip/7za.exe" CACHE FILEPATH "Path to prebuilt 7za.exe")
    set(PREBUILT_7ZA_EXE_ALT "${CMAKE_SOURCE_DIR}/third_party/7zip/bin/7za.exe" CACHE FILEPATH "Alternative path to prebuilt 7za.exe")

    if (BACKUPSUITE_USE_PREBUILT_7Z_DLL)
        set(_sevenzip_bins)
        foreach (_cand IN LISTS PREBUILT_7Z_DLL PREBUILT_7Z_DLL_ALT PREBUILT_7ZIP_DLL PREBUILT_7Z_EXE PREBUILT_7Z_EXE_ALT PREBUILT_7ZA_EXE PREBUILT_7ZA_EXE_ALT)
            if (EXISTS "${_cand}")
                list(APPEND _sevenzip_bins "${_cand}")
            endif ()
        endforeach ()
        if (_sevenzip_bins)
            add_custom_command(TARGET backup_suite_core POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E echo "Copying prebuilt 7z binaries: ${_sevenzip_bins}"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_sevenzip_bins} $<TARGET_FILE_DIR:backup_suite_core>
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_sevenzip_bins} ${BIN_CLI_DIR}
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_sevenzip_bins} ${BIN_GUI_DIR}
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_sevenzip_bins} ${BIN_TESTS_DIR}
            )
        else ()
            message(STATUS "Prebuilt 7z DLLs not found; fallback to building p7zip sources")
        endif ()
    endif ()

    if (NOT (_sevenzip_bins))
        # Build a minimal subset of p7zip as a static library
        # Note: p7zip 源码布局较大，下面选择常见 7z 容器/加密/通用模块目录
        file(GLOB_RECURSE P7Z_COMMON_SRC
                "${BACKUPSUITE_P7ZIP_SOURCE_DIR}/CPP/Common/*.cpp"
                "${BACKUPSUITE_P7ZIP_SOURCE_DIR}/CPP/Windows/*.cpp"
                "${BACKUPSUITE_P7ZIP_SOURCE_DIR}/CPP/7zip/Common/*.cpp"
        )
        file(GLOB_RECURSE P7Z_ARCHIVE_7Z_SRC
                "${BACKUPSUITE_P7ZIP_SOURCE_DIR}/CPP/7zip/Archive/7z/*.cpp"
        )
        file(GLOB_RECURSE P7Z_CRYPTO_SRC
                "${BACKUPSUITE_P7ZIP_SOURCE_DIR}/CPP/7zip/Crypto/*.cpp"
        )

        # 过滤可能是可执行入口或不需要的工具代码（保守忽略常见 Main/Bench/Extract/Compress 等）
        list(FILTER P7Z_COMMON_SRC EXCLUDE REGEX ".*/(Main|Bench|Extract|Compress)\\.cpp$")
        list(FILTER P7Z_ARCHIVE_7Z_SRC EXCLUDE REGEX ".*/(Main|Bench|Extract|Compress)\\.cpp$")
        list(FILTER P7Z_COMMON_SRC EXCLUDE REGEX ".*/Windows/System\\.cpp$")

        add_library(thirdparty_p7zip STATIC
                ${P7Z_COMMON_SRC}
                ${P7Z_ARCHIVE_7Z_SRC}
                ${P7Z_CRYPTO_SRC}
        )
        target_include_directories(thirdparty_p7zip PUBLIC
                "${BACKUPSUITE_P7ZIP_SOURCE_DIR}/CPP"
                "${BACKUPSUITE_P7ZIP_SOURCE_DIR}/CPP/myWindows"
                "${BACKUPSUITE_P7ZIP_SOURCE_DIR}/C"
        )
        # 常用编译定义：启用UNICODE，单线程后端（可根据需要调整）
        target_compile_definitions(thirdparty_p7zip PUBLIC UNICODE _UNICODE _7ZIP_ST NOMINMAX _CRT_SECURE_NO_WARNINGS)
        if (MSVC)
            target_compile_options(thirdparty_p7zip PRIVATE /utf-8 /W3 /UENV_HAVE_PTHREAD)
        else ()
            target_compile_options(thirdparty_p7zip PRIVATE -Wall -UENV_HAVE_PTHREAD)
        endif ()

        # 链接到核心库
        target_link_libraries(backup_suite_core PUBLIC thirdparty_p7zip)
    endif ()
endif ()

if (MSVC)
    target_compile_options(backup_suite_core PRIVATE /utf-8 /W3)
    target_compile_options(backup_suite_core PRIVATE /wd4251 /wd4244)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else ()
    target_compile_options(backup_suite_core PRIVATE -Wall)
endif ()
