cmake_minimum_required(VERSION 2.8)
project(bladeRF-fsk C)

################################################################################
# Version information
################################################################################

set(VERSION_INFO_MAJOR  0)
set(VERSION_INFO_MINOR  1)
set(VERSION_INFO_PATCH  0)

################################################################################
# Dependencies
################################################################################

set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

if(MSVC)
    find_package(LibPThreadsWin32 REQUIRED)
else(MSVC)
	find_package(Threads REQUIRED)
endif(MSVC)

################################################################################
# Options
################################################################################
option(BLADERF-FSK_ENABLE_DEBUG_ALL "Print debug messages in all files" OFF)
option(BLADERF-FSK_ENABLE_DEBUG_PHY
        "Print debug messages in phy.c"
        ${BLADERF-FSK_ENABLE_DEBUG_ALL}
)
option(BLADERF-FSK_ENABLE_DEBUG_LINK
        "Print debug messages in link.c"
        ${BLADERF-FSK_ENABLE_DEBUG_ALL}
)
option(BLADERF-FSK_ENABLE_DEBUG_FSK
        "Print debug messages in fsk.c"
        ${BLADERF-FSK_ENABLE_DEBUG_ALL}
)
option(
        BLADERF-FSK_ENABLE_DEBUG_TEST_SUITE
        "Print debug messages in test_suite.c"
        ${BLADERF-FSK_ENABLE_DEBUG_ALL}
)
option(BLADERF-FSK_ENABLE_DEBUG_BLADERF
        "Print libbladeRF debug messages"
        ${BLADERF-FSK_ENABLE_DEBUG_ALL}
)
option(BLADERF-FSK_ENABLE_NOTES_PHY
        "Print noteworthy messages (e.g. recoverable errors) in phy.c"
        OFF
)
option(BLADERF-FSK_ENABLE_NOTES_LINK
        "Print noteworthy messages (e.g. recoverable errors) in link.c"
        OFF
)
option(BLADERF-FSK_BYPASS_RX_CHANNEL_FILTER "Bypass the PHY's RX channel filter." OFF)
option(BLADERF-FSK_BYPASS_PHY_SCRAMBLING "Bypass scrambling in the phy layer" OFF)
option(BLADERF-FSK_BYPASS_RX_PNORM "Bypass the PHY's RX power normalization" OFF)

if(BLADERF-FSK_ENABLE_DEBUG_PHY)
    set_property(SOURCE ${SRC_DIR}/phy.c APPEND_STRING PROPERTY COMPILE_FLAGS "-DDEBUG_MODE ")
endif()
if(BLADERF-FSK_ENABLE_DEBUG_LINK)
    set_property(SOURCE ${SRC_DIR}/link.c APPEND_STRING PROPERTY COMPILE_FLAGS "-DDEBUG_MODE ")
endif()
if(BLADERF-FSK_ENABLE_DEBUG_FSK)
    set_property(SOURCE ${SRC_DIR}/fsk.c APPEND_STRING PROPERTY COMPILE_FLAGS "-DDEBUG_MODE ")
endif()
if(BLADERF-FSK_ENABLE_DEBUG_BLADERF)
    set_property(SOURCE ${SRC_DIR}/radio_config.c APPEND_STRING PROPERTY COMPILE_FLAGS "-DDEBUG_MODE ")
endif()
if(BLADERF-FSK_ENABLE_DEBUG_TEST_SUITE)
    set_property(SOURCE ${SRC_DIR}/test_suite.c APPEND_STRING PROPERTY COMPILE_FLAGS "-DDEBUG_MODE ")
endif()

if(BLADERF-FSK_ENABLE_NOTES_PHY)
    set_property(SOURCE ${SRC_DIR}/phy.c APPEND_STRING PROPERTY COMPILE_FLAGS "-DENABLE_NOTES ")
endif()

if(BLADERF-FSK_ENABLE_NOTES_LINK)
    set_property(SOURCE ${SRC_DIR}/link.c APPEND_STRING PROPERTY COMPILE_FLAGS "-DENABLE_NOTES ")
endif()

if(BLADERF-FSK_BYPASS_RX_CHANNEL_FILTER)
    set_property(SOURCE ${SRC_DIR}/phy.c APPEND_STRING PROPERTY COMPILE_FLAGS "-DBYPASS_RX_CHANNEL_FILTER ")
endif()
if(BLADERF-FSK_BYPASS_PHY_SCRAMBLING)
    set_property(SOURCE ${SRC_DIR}/phy.c APPEND_STRING PROPERTY COMPILE_FLAGS "-DBYPASS_PHY_SCRAMBLING ")
endif()
if(BLADERF-FSK_BYPASS_RX_PNORM)
    set_property(SOURCE ${SRC_DIR}/phy.c APPEND_STRING PROPERTY COMPILE_FLAGS "-DBYPASS_RX_PNORM ")
endif()

################################################################################
# Include paths
################################################################################
set (FSK_INCLUDE_DIRS
        ${SRC_DIR}
        ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
        ${libbladeRF_SOURCE_DIR}/include
)

if(MSVC)
    set(FSK_INCLUDE_DIRS ${FSK_INCLUDE_DIRS} ${MSVC_C99_INCLUDES})
    set(FSK_INCLUDE_DIRS ${FSK_INCLUDE_DIRS}
        ${BLADERF_HOST_COMMON_INCLUDE_DIRS}/windows
        ${LIBPTHREADSWIN32_INCLUDE_DIRS}
    )
endif()

if(APPLE)
    set(FSK_INCLUDE_DIRS ${FSK_INCLUDE_DIRS}
        ${BLADERF_HOST_COMMON_INCLUDE_DIRS}/osx
    )
endif()

include_directories(${FSK_INCLUDE_DIRS})

################################################################################
# Main bladeRF-fsk program
################################################################################

set(BLADERF_FSK_SRC
    ${SRC_DIR}/bladeRF-fsk.c
    ${SRC_DIR}/config.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
    ${SRC_DIR}/radio_config.c
    ${SRC_DIR}/fir_filter.c
    ${SRC_DIR}/fsk.c
    ${SRC_DIR}/prng.c
    ${SRC_DIR}/phy.c
    ${SRC_DIR}/crc32.c
    ${SRC_DIR}/link.c
    ${SRC_DIR}/utils.c
    ${SRC_DIR}/pnorm.c
    ${SRC_DIR}/correlator.c
)

if(MSVC)
    set(BLADERF_FSK_SRC ${BLADERF_FSK_SRC}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/clock_gettime.c
    )
endif()

if(APPLE)
    set(BLADERF_FSK_SRC ${BLADERF_FSK_SRC}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/osx/clock_gettime.c
    )
endif()

# Set link libraries
set(BLADERF_FSK_LIBS
    libbladerf_shared
    ${CMAKE_THREAD_LIBS_INIT}
)

if(NOT MSVC)
    set(BLADERF_FSK_LIBS ${BLADERF_FSK_LIBS} m)
endif()

if(LIBPTHREADSWIN32_FOUND)
    set(BLADERF_FSK_LIBS ${BLADERF_FSK_LIBS} ${LIBPTHREADSWIN32_LIBRARIES})
endif()

if(LIBC_VERSION)
    # clock_gettime() was moved from librt -> libc in 2.17
    if(${LIBC_VERSION} VERSION_LESS "2.17")
        set(BLADERF_FSK_LIBS ${BLADERF_FSK_LIBS} rt)
    endif()
endif()

add_executable(bladeRF-fsk ${BLADERF_FSK_SRC})
target_link_libraries(bladeRF-fsk ${BLADERF_FSK_LIBS})

################################################################################
# Installation of bladeRF-fsk program
################################################################################
if (NOT DEFINED BIN_INSTALL_DIR)
    set(BIN_INSTALL_DIR bin)
endif()

install(TARGETS bladeRF-fsk DESTINATION ${BIN_INSTALL_DIR})

################################################################################
# Test suite program
################################################################################

set(TEST_SUITE_SRC
    ${SRC_DIR}/radio_config.c
    ${SRC_DIR}/fir_filter.c
    ${SRC_DIR}/fsk.c
    ${SRC_DIR}/prng.c
    ${SRC_DIR}/crc32.c
    ${SRC_DIR}/phy.c
    ${SRC_DIR}/link.c
    ${SRC_DIR}/test_suite.c
    ${SRC_DIR}/utils.c
    ${SRC_DIR}/pnorm.c
    ${SRC_DIR}/correlator.c
)

if(MSVC)
    set(TEST_SUITE_SRC ${TEST_SUITE_SRC}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/clock_gettime.c
    )
endif()

if(APPLE)
    set(TEST_SUITE_SRC ${TEST_SUITE_SRC}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/osx/clock_gettime.c
    )
endif()

# Set link libraries
set(TEST_SUITE_LIBS
    libbladerf_shared
    ${CMAKE_THREAD_LIBS_INIT}
)

if(NOT MSVC)
    set(TEST_SUITE_LIBS ${TEST_SUITE_LIBS} m)
endif()

if(LIBPTHREADSWIN32_FOUND)
    set(TEST_SUITE_LIBS ${TEST_SUITE_LIBS} ${LIBPTHREADSWIN32_LIBRARIES})
endif()

if(LIBC_VERSION)
    # clock_gettime() was moved from librt -> libc in 2.17
    if(${LIBC_VERSION} VERSION_LESS "2.17")
        set(TEST_SUITE_LIBS ${TEST_SUITE_LIBS} rt)
    endif()
endif()

add_executable(bladeRF-fsk_test_suite ${TEST_SUITE_SRC})
target_link_libraries(bladeRF-fsk_test_suite ${TEST_SUITE_LIBS})

################################################################################
# Configuration test
################################################################################

set(TEST_CONFIG_SRC
    ${SRC_DIR}/config.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
)

if(MSVC)
    set(TEST_CONFIG_SRC ${TEST_CONFIG_SRC}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
    )
endif()

# Set link libraries
set(TEST_CONFIG_LIBS
    libbladerf_shared
)

add_executable(bladeRF-fsk_test_config ${TEST_CONFIG_SRC})
target_compile_definitions(bladeRF-fsk_test_config PRIVATE "-DCONFIG_TEST")
target_link_libraries(bladeRF-fsk_test_config ${TEST_CONFIG_LIBS})

################################################################################
# CRC32 helpers and tools
################################################################################

set(CRC32_TEST_SRC ${SRC_DIR}/crc32.c)
add_executable(bladeRF-fsk_test_crc32 ${CRC32_TEST_SRC})
target_compile_definitions(bladeRF-fsk_test_crc32 PRIVATE "-DCRC32_TEST")

################################################################################
# PRNG test
################################################################################

set(PRNG_TEST_SRC ${SRC_DIR}/prng.c)
add_executable(bladeRF-fsk_test_prng ${PRNG_TEST_SRC})
target_compile_definitions(bladeRF-fsk_test_prng PRIVATE "-DPRNG_TEST")

################################################################################
# FIR Filter test
################################################################################

set(FIR_FILTER_TEST_SRC 
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
    ${SRC_DIR}/fir_filter.c
    ${SRC_DIR}/utils.c
)

if(MSVC)
    set(FIR_FILTER_TEST_SRC ${FIR_FILTER_TEST_SRC}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/clock_gettime.c
    )
endif()

if(APPLE)
    set(FIR_FILTER_TEST_SRC ${FIR_FILTER_TEST_SRC}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/osx/clock_gettime.c
    )
endif()

# Set link libraries
if(NOT MSVC)
    set(FIR_FILTER_TEST_LIBS ${FIR_FILTER_TEST_LIBS} m)
endif()

if(LIBC_VERSION)
    # clock_gettime() was moved from librt -> libc in 2.17
    if(${LIBC_VERSION} VERSION_LESS "2.17")
        set(FIR_FILTER_TEST_LIBS ${FIR_FILTER_TEST_LIBS} rt)
    endif()
endif()

add_executable(bladeRF-fsk_test_fir_filter ${FIR_FILTER_TEST_SRC})
target_compile_definitions(bladeRF-fsk_test_fir_filter PRIVATE "-DFIR_FILTER_TEST")
target_link_libraries(bladeRF-fsk_test_fir_filter ${FIR_FILTER_TEST_LIBS})

################################################################################
# Correlator test
################################################################################

set(CORRELATOR_TEST_SRC 
    ${SRC_DIR}/correlator.c
    ${SRC_DIR}/utils.c
    ${SRC_DIR}/fsk.c
)

if(MSVC)
    set(CORRELATOR_TEST_SRC ${CORRELATOR_TEST_SRC}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/clock_gettime.c
    )
endif()

if(APPLE)
    set(CORRELATOR_TEST_SRC ${CORRELATOR_TEST_SRC}
            ${BLADERF_HOST_COMMON_SOURCE_DIR}/osx/clock_gettime.c
    )
endif()

# Set link libraries
if(NOT MSVC)
    set(CORRELATOR_TEST_LIBS ${CORRELATOR_TEST_LIBS} m)
endif()

if(LIBC_VERSION)
    # clock_gettime() was moved from librt -> libc in 2.17
    if(${LIBC_VERSION} VERSION_LESS "2.17")
        set(CORRELATOR_TEST_LIBS ${CORRELATOR_TEST_LIBS} rt)
    endif()
endif()

add_executable(bladeRF-fsk_test_correlator ${CORRELATOR_TEST_SRC})
target_compile_definitions(bladeRF-fsk_test_correlator PRIVATE "-DCORRELATOR_TEST")
target_link_libraries(bladeRF-fsk_test_correlator ${CORRELATOR_TEST_LIBS})

################################################################################
# Power normalizer test
################################################################################
set(PNORM_TEST_SRC ${SRC_DIR}/pnorm.c)

# Set link libraries
if(NOT MSVC)
    set(PNORM_TEST_LIBS ${PNORM_TEST_LIBS} m)
endif()

add_executable(bladeRF-fsk_test_pnorm ${PNORM_TEST_SRC})
target_compile_definitions(bladeRF-fsk_test_pnorm PRIVATE "-DPNORM_TEST")
target_link_libraries(bladeRF-fsk_test_pnorm ${PNORM_TEST_LIBS})
