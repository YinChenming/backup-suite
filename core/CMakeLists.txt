include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(CORE_INCLUDES
        api.h
        device.h
        entities.h
        system_device.h
        utils/time_converter.h
        utils/tar.h
        utils/admin_privilege.h
)

set(CORE_SOURCES
        src/filesystem/system_device.cpp
        src/utils/time_converter.cpp
        src/utils/tar.cpp
        src/utils/admin_privilege.cpp
)

# 将include目录添加到项目中
add_library(backup_suite_core SHARED ${CURE_INCLUDES} ${CORE_SOURCES})
target_compile_definitions(backup_suite_core PRIVATE BACKUP_SUITE_CORE_EXPORTS)

target_include_directories(backup_suite_core
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
