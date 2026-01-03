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
    if (NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party")
        file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party")
    endif ()

    if (BACKUPSUITE_USE_PREBUILT_7Z_DLL)
        set(_sevenzip_bins)
        foreach (_cand IN LISTS PREBUILT_7Z_DLL PREBUILT_7Z_DLL_ALT PREBUILT_7ZIP_DLL PREBUILT_7Z_EXE PREBUILT_7Z_EXE_ALT PREBUILT_7ZA_EXE PREBUILT_7ZA_EXE_ALT)
            if (EXISTS "${_cand}")
                list(APPEND _sevenzip_bins "${_cand}")
            endif ()
        endforeach ()

        # Auto-fetch official 7-Zip extra package if nothing is found
        if (NOT _sevenzip_bins)
            set(_sevenzip_dir "${CMAKE_SOURCE_DIR}/third_party/7-Zip")
            file(MAKE_DIRECTORY "${_sevenzip_dir}")
            set(_sevenzip_extra_url "https://www.7-zip.org/a/7z2501-extra.7z")
            set(_sevenzip_7zr_url   "https://www.7-zip.org/a/7zr.exe")
            set(_sevenzip_extra_archive "${_sevenzip_dir}/7z-extra.7z")
            set(_sevenzip_7zr_exe "${_sevenzip_dir}/7zr.exe")

            file(DOWNLOAD "${_sevenzip_7zr_url}" "${_sevenzip_7zr_exe}" SHOW_PROGRESS STATUS _dl_7zr)
            list(GET _dl_7zr 0 _dl_7zr_code)
            if (NOT _dl_7zr_code EQUAL 0)
                message(FATAL_ERROR "Failed to download 7zr.exe from ${_sevenzip_7zr_url} (status: ${_dl_7zr})")
            endif ()

            file(DOWNLOAD "${_sevenzip_extra_url}" "${_sevenzip_extra_archive}" SHOW_PROGRESS STATUS _dl_extra)
            list(GET _dl_extra 0 _dl_extra_code)
            if (NOT _dl_extra_code EQUAL 0)
                message(FATAL_ERROR "Failed to download 7-Zip extra archive from ${_sevenzip_extra_url} (status: ${_dl_extra})")
            endif ()

            execute_process(
                    COMMAND "${_sevenzip_7zr_exe}" x "${_sevenzip_extra_archive}" -y
                    WORKING_DIRECTORY "${_sevenzip_dir}"
                    RESULT_VARIABLE _7zr_res
                    OUTPUT_VARIABLE _7zr_out
                    ERROR_VARIABLE _7zr_err)
            if (NOT _7zr_res EQUAL 0)
                message(FATAL_ERROR "Failed to extract 7-Zip extra archive with 7zr (code ${_7zr_res})\nstdout: ${_7zr_out}\nstderr: ${_7zr_err}")
            endif ()

            foreach (_cand IN LISTS PREBUILT_7Z_DLL PREBUILT_7Z_DLL_ALT PREBUILT_7ZIP_DLL PREBUILT_7Z_EXE PREBUILT_7Z_EXE_ALT PREBUILT_7ZA_EXE PREBUILT_7ZA_EXE_ALT)
                if (EXISTS "${_cand}")
                    list(APPEND _sevenzip_bins "${_cand}")
                endif ()
            endforeach ()
            # If names differ in the extra package, pick up any freshly extracted exe/dll as fallback
            file(GLOB _extra_bins
                     "${_sevenzip_dir}/*.dll"
                     "${_sevenzip_dir}/*.exe")
            list(APPEND _sevenzip_bins ${_extra_bins})
        endif ()

        # Keep only existing files (dll/exe) to avoid copy errors
        set(_sevenzip_bins_filtered)
        foreach(_b IN LISTS _sevenzip_bins)
            if (EXISTS "${_b}" AND NOT IS_DIRECTORY "${_b}")
                get_filename_component(_ext "${_b}" EXT)
                if (_ext STREQUAL ".dll" OR _ext STREQUAL ".exe")
                    list(APPEND _sevenzip_bins_filtered "${_b}")
                endif()
            endif()
        endforeach()
        list(REMOVE_DUPLICATES _sevenzip_bins_filtered)
        set(_sevenzip_bins ${_sevenzip_bins_filtered})

        if (_sevenzip_bins)
            message(STATUS "Using 7z artifacts: ${_sevenzip_bins}")
            add_custom_command(TARGET backup_suite_core POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${BIN_CLI_DIR}
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${BIN_GUI_DIR}
                    COMMAND ${CMAKE_COMMAND} -E make_directory ${BIN_TESTS_DIR}
                    COMMAND ${CMAKE_COMMAND} -E echo "Copying prebuilt 7z binaries: ${_sevenzip_bins}"
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_sevenzip_bins} $<TARGET_FILE_DIR:backup_suite_core>
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_sevenzip_bins} ${BIN_CLI_DIR}
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_sevenzip_bins} ${BIN_GUI_DIR}
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${_sevenzip_bins} ${BIN_TESTS_DIR}
            )
        else ()
            message(FATAL_ERROR "Prebuilt 7z binaries not found and automatic download failed. Please place 7z.dll/7z.exe into third_party/7-Zip or third_party/7zip/bin.")
        endif ()
    endif ()
endif ()

if (MSVC)
    target_compile_options(backup_suite_core PRIVATE /utf-8)
    target_compile_options(backup_suite_core PRIVATE /wd4251 /wd4244)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else ()
    target_compile_options(backup_suite_core PRIVATE -Wall)
endif ()
