cmake_minimum_required(VERSION 3.10...3.27)
project(libbladeRF_test_packet C)

set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    ${libbladeRF_SOURCE_DIR}/include
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
)

# Checks if the include directories exist
foreach(dir ${INCLUDES})
  if(NOT EXISTS ${dir})
    message(FATAL_ERROR "The include directory '${dir}' does not exist.")
  endif()
endforeach()

set(SRC
    src/main.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c)

if(MSVC)
    set(SRC ${SRC}
        ${BLADERF_HOST_COMMON_SOURCE_DIR}/windows/getopt_long.c
    )
endif()

include_directories(${INCLUDES})
add_executable(${PROJECT_NAME} ${SRC})
target_link_libraries(${PROJECT_NAME} libbladerf_shared)

