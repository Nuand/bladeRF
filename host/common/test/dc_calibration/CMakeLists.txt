cmake_minimum_required(VERSION 2.8)
project(test_dc_calibration C)

set(INCLUDES 
    ${libbladeRF_SOURCE_DIR}/include
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
)

set(LIBS libbladerf_shared)

if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
else()
    set(LIBS ${LIBS} m)
endif()

set(SRC 
    src/main.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/dc_calibration.c
)

include_directories(${INCLUDES})
add_executable(test_dc_calibration ${SRC})
target_link_libraries(test_dc_calibration ${LIBS})
