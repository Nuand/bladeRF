cmake_minimum_required(VERSION 3.11...3.27)  # Increased for FetchContent
project(bladeRF-update C)

include(FetchContent)

set(INCLUDES
    ${libbladeRF_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Platform-specific configurations
if(WIN32)
    list(APPEND LIBS wininet)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
elseif(UNIX)
    find_package(CURL REQUIRED)
    list(APPEND INCLUDES ${CURL_INCLUDE_DIRS})
    list(APPEND LIBS ${CURL_LIBRARIES})
endif()

set(JSON_C_SOURCES "")

add_executable(bladeRF-update
    bladeRF-update.c
    bladeRF_json.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/sha256.c
    ${JSON_C_SOURCES}
)

# Add libbladeRF to the list of libraries
list(APPEND LIBS libbladerf_shared)

target_link_libraries(bladeRF-update ${LIBS})
target_include_directories(bladeRF-update PUBLIC ${INCLUDES})

install(TARGETS bladeRF-update DESTINATION ${CMAKE_INSTALL_BINDIR})
