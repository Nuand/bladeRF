cmake_minimum_required(VERSION 3.11) # FetchContent is available in 3.11
project(libbladeRF_test_regression
        VERSION 3.11
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

include(GoogleTest)
include(FetchContent)
enable_testing()

FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.13.x
)

FetchContent_GetProperties(googletest)
if(NOT googletest_POPULATED)
  FetchContent_Populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR})
endif()
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(ABSL_PROPAGATE_CXX_STD ON)
FetchContent_Declare(
  abseil
  GIT_REPOSITORY https://github.com/abseil/abseil-cpp.git
  GIT_TAG lts_2023_01_25
)
if(NOT abseil_POPULATED)
  FetchContent_Populate(abseil)
  add_subdirectory(${abseil_SOURCE_DIR} ${abseil_BINARY_DIR})
endif()

set(INCLUDES
    ./include
    ${libbladeRF_SOURCE_DIR}/include
    ${libbladeRF_SOURCE_DIR}/src
    ${BLADERF_HOST_COMMON_INCLUDE_DIRS}
)
include_directories(${INCLUDES})

set(SRC
    src/main.cxx
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/log.c
    ${BLADERF_HOST_COMMON_SOURCE_DIR}/conversions.c
)
add_executable(${PROJECT_NAME} ${SRC})

add_custom_target(test
                  COMMAND ${CMAKE_COMMAND} -E echo ""
                  COMMAND ${CMAKE_COMMAND} -E echo "Available test targets:"
                  COMMAND ${CMAKE_COMMAND} -E echo "  make test_all        - Run all tests"
                  COMMAND ${CMAKE_COMMAND} -E echo "  make test_hdl        - Run tests with filter 'hdl*'"
                  COMMAND ${CMAKE_COMMAND} -E echo "  make test_libbladeRF - Run tests with filter 'libbladeRF*'"
                  COMMAND ${CMAKE_COMMAND} -E echo "  make test_synthesis  - Run tests with filter 'synthesize*'"
                  COMMAND ${CMAKE_COMMAND} -E echo ""
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_custom_target(test_all
                  COMMAND ${PROJECT_NAME} --gtest_output="json"
                  DEPENDS ${PROJECT_NAME}
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_custom_target(test_hdl
                  COMMAND ${PROJECT_NAME} --gtest_filter="hdl*" --gtest_output="json"
                  DEPENDS ${PROJECT_NAME}
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_custom_target(test_libbladeRF
                  COMMAND ${PROJECT_NAME} --gtest_filter="libbladeRF*" --gtest_output="json"
                  DEPENDS ${PROJECT_NAME}
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

add_custom_target(test_synthesis
                  COMMAND ${PROJECT_NAME} --gtest_filter="synthesize*" --gtest_output="json"
                  DEPENDS ${PROJECT_NAME}
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR})

target_link_libraries(
    ${PROJECT_NAME}
    libbladerf_shared
    gtest_main
    absl::strings
)

################################################################
# Find the nios2_command_shell
################################################################
find_program(NIOS2_COMMAND_SHELL_EXECUTABLE nios2_command_shell.sh
    PATHS ~/intelFPGA_lite/20.1/nios2eds
    NO_CACHE
)
