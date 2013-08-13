# Copyright 2012 Nuand, LLC.
#
# This file is part of the bladeRF project
#
# TODO license text here (see top-level COPYING for time being)
cmake_minimum_required(VERSION 2.8)

option(INSTALL_UDEV_RULES 
    "Install udev rules for the nuand bladeRF" 
    OFF)

if(NOT CMAKE_HOST_UNIX)
    if(INSTALL_UDEV_RULES)
        message(FATAL_ERROR "INSTALL_UDEV_RULES=ON is not valid for this OS.")
    endif(INSTALL_UDEV_RULES)
else()
    if(INSTALL_UDEV_RULES)
        message(STATUS  "Installing nuand bladeRF udev rules")
        install(FILES 88-nuand.rules 
                DESTINATION /etc/udev/rules.d 
                COMPONENT "udev_rules")
    else()
        message(STATUS
                "Not installing nuand bladeRF udev rules. Set INSTALL_UDEV_RULES=ON to install." 
               )
    endif(INSTALL_UDEV_RULES)
endif()
