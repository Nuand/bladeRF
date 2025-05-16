cmake_minimum_required(VERSION 3.10...3.27)
project(libbladeRF_test_oversample C)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../common/include
)
if(MSVC)
    set(INCLUDES ${INCLUDES} ${MSVC_C99_INCLUDES})
endif()

include_directories(${INCLUDES})
add_executable(${PROJECT_NAME} src/main.c ../common/src/test_common.c)
target_link_libraries(${PROJECT_NAME}
    libbladerf_shared
    m
)

add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
