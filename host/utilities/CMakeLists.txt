cmake_minimum_required(VERSION 2.8)
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
