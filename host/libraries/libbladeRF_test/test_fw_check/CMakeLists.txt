cmake_minimum_required(VERSION 2.8)
project(libbladeRF_test_fw_check C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${libbladeRF_SOURCE_DIR}/src
    ${BLADERF_FW_COMMON_INCLUDE_DIR}
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
)
if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
endif()

add_definitions(-DLOGGING_ENABLED=1)

if(LIBBLADERF_SEARCH_PREFIX_OVERRIDE)
    add_definitions(-DLIBBLADERF_SEARCH_PREFIX="${LIBBLADERF_SEARCH_PREFIX_OVERRIDE}")
else()
    add_definitions(-DLIBBLADERF_SEARCH_PREFIX="${CMAKE_INSTALL_PREFIX}")
endif()

if(MSVC)
    find_package(LibPThreadsWin32 REQUIRED)
    set(INCLUDES ${INCLUDES} ${LIBPTHREADSWIN32_INCLUDE_DIRS})
    set(LIBS ${LIBS} ${LIBPTHREADSWIN32_LIBRARIES})
else(MSVC)
    find_package(Threads REQUIRED)
    set(LIBS ${LIBS} ${CMAKE_THREAD_LIBS_INIT})
endif(MSVC)

set(SRC
    src/main.c
    ${libbladeRF_SOURCE_DIR}/src/driver/fx3_fw.c
    ${libbladeRF_SOURCE_DIR}/src/helpers/file.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
)

add_definitions(-DLOGGING_ENABLED)
add_definitions(-DTEST_FX3_FW_VALIDATION)
add_definitions(-DLIBBLADERF_SEARCH_PREFIX="${CMAKE_INSTALL_PREFIX}")

include_directories(${INCLUDES})
add_executable(libbladeRF_test_fw_check ${SRC})
target_link_libraries(libbladeRF_test_fw_check libbladerf_shared)
