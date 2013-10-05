cmake_minimum_required(VERSION 2.8)

set(UTILITIES_COMMON_SRC ${CMAKE_CURRENT_SOURCE_DIR}/common/src)
set(UTILITIES_COMMON_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/common/include)

add_subdirectory(bladeRF-cli)

if(ENABLE_BACKEND_LIBUSB)
    find_package(LibUSB)
    if(LIBUSB_FOUND)
        if(LIBUSB_HAVE_HOTPLUG)
            add_subdirectory(bladeRF-flash)
        else()
            message(WARNING "Installed libusb does not contain hotplug support. Disabling build of bladeRF-flash.")
        endif(LIBUSB_HAVE_HOTPLUG)
    endif(LIBUSB_FOUND)
endif(ENABLE_BACKEND_LIBUSB)

