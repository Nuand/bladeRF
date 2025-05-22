cmake_minimum_required(VERSION 3.10...3.27)
project(libbladeRF_test_oversample C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include/windows
)
if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
endif()

set(SRC
    src/main.c
    ../common/src/test_common.c
)

if(MSVC OR WIN32)
    set(SRC ${SRC}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/src/windows/getopt_long.c
        ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/src/windows/clock_gettime.c
    )
endif()

include_directories(${INCLUDES})
add_executable(${PROJECT_NAME} ${SRC})
target_link_libraries(${PROJECT_NAME}
    libbladerf_shared
)

# Only link with the math library on non-Windows platforms
if(NOT WIN32 AND NOT MSVC)
    target_link_libraries(${PROJECT_NAME} m)
endif()

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
