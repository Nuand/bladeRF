cmake_minimum_required(VERSION 3.10...3.27)
project(libbladeRF_test_async C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${libbladeRF_SOURCE_DIR}/src
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
)

if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
endif()

include_directories(${INCLUDES})
add_executable(libbladeRF_test_async main.c)
target_link_libraries(libbladeRF_test_async libbladerf_shared)

add_executable(libbladeRF_test_async_packed packed.c)
target_link_libraries(libbladeRF_test_async_packed libbladerf_shared)
