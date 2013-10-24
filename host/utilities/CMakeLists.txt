cmake_minimum_required(VERSION 2.8)

set(UTILITIES_COMMON_SRC ${CMAKE_CURRENT_SOURCE_DIR}/common/src)
set(UTILITIES_COMMON_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common/include)

add_subdirectory(bladeRF-cli)


if(ENABLE_BACKEND_LIBUSB)
    find_package(LibUSB)
    include(CheckFunctionExists)
    set(CMAKE_REQUIRED_INCLUDES libusb.h)
    set(CMAKE_REQUIRED_LIBRARIES ${LIBUSB_LIBRARIES})
    check_function_exists(libusb_hotplug_register_callback HAVE_LIBUSB_HOTPLUG)
    unset(CMAKE_REQUIRED_INCLUDES)
    unset(CMAKE_REQUIRED_LIBRARIES)
endif(ENABLE_BACKEND_LIBUSB)

if(${HAVE_LIBUSB_HOTPLUG} MATCHES "1")
    add_subdirectory(bladeRF-flash)
else(${HAVE_LIBUSB_HOTPLUG} MATCHES "1")
    if(WIN32)
        find_package(LibUSB)
        try_run(RET_RUN RET_COM
            ${CMAKE_CURRENT_BINARY_DIR}
            "${CMAKE_CURRENT_SOURCE_DIR}/ver.c"
            CMAKE_FLAGS "-DINCLUDE_DIRECTORIES:STRING=${LIBUSB_INCLUDE_DIRS}"
            RUN_OUTPUT_VARIABLE OUTPUT)

        if(NOT ${RET_RUN} MATCHES "0" OR NOT ${RET_COM} MATCHES "TRUE")
            message("Could not compile and run libusbx version test")
        endif()

        if(${OUTPUT} MATCHES "1")
            message("Found the correct version of libusbx using alternate method")
        else(${OUTPUT} MATCHES "1")
            message(FATAL_ERROR "The version of libusbx that you are using is too old\n"
                    "Follow this guide: https://github.com/Nuand/bladeRF/wiki/Getting-Started%3A-Windows#installing-libusbx"
                    )
        endif()
    else(WIN32)
        message("Your libusb version is out of date. Please follow the instructions at\n"
               "https://github.com/Nuand/bladeRF/wiki/Debian-setup-guide#build-libusb-tools-and-libraries-from-nuand-git-master")
    endif()
endif(${HAVE_LIBUSB_HOTPLUG} MATCHES "1")

################################################################################
# Doxygen documentation, for man pages
################################################################################
find_package(Doxygen)

if(DOXYGEN_FOUND)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/doc/doxygen/Doxyfile.in
    ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/Doxyfile
    @ONLY
)

# Target to generate the man pages.
# Note that doxygen prefers to output to lower-cased filenames.  This annoys
# lintian, for obvious reasons.  So, we create copies with the correct case.
add_custom_target(utilities_manpages ALL
    COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen
    COMMENT "Generating bladeRF utilities man pages via Doxygen in: ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen"
)

if(NOT DEFINED MAN_INSTALL_DIR)
	set(MAN_INSTALL_DIR share/man)
endif(NOT DEFINED MAN_INSTALL_DIR)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/man/man1/bladerf-cli.1
    ${CMAKE_CURRENT_BINARY_DIR}/doc/doxygen/man/man1/bladerf-flash.1
    DESTINATION ${MAN_INSTALL_DIR}/man1/)

endif(DOXYGEN_FOUND)
