# Copyright 2013 Nuand, LLC.
#
# This file is part of the bladeRF project
#
# TODO license text here (see top-level COPYING for time being)
cmake_minimum_required(VERSION 2.8)

option(INSTALL_UDEV_RULES
    "Install udev rules for the nuand bladeRF"
    ON)

if(NOT CMAKE_HOST_UNIX)
    if(INSTALL_UDEV_RULES)
        message(STATUS "udev rules not supported on this platform. Hide this message via -DINSTALL_UDEV_RULES=Off")
    endif(INSTALL_UDEV_RULES)
else()
    if(INSTALL_UDEV_RULES)
        message(STATUS  "nuand bladeRF udev rules will be installed upon running 'make install'")

	# Default group is root if no better match is found below
	set (BLADERF_GROUP root)

	# Find and execute lsb_release to get the distro name
	find_program(LSB_RELEASE lsb_release
		PATH /bin /sbin /usr/bin /usr/sbin /usr/local/bin /usr/local/sbin)
	if (LSB_RELEASE)
		execute_process(COMMAND ${LSB_RELEASE} -si
			OUTPUT_VARIABLE LINUX_DISTRO_NAME)
	endif()

	# Strip the line break
	string(REPLACE "\n" "" LINUX_DISTRO_NAME ${LINUX_DISTRO_NAME})

	# Arch Linux
	# lsb_release is not in base, so we'll check for a release file
	if (EXISTS /etc/arch-release)
		set (BLADERF_GROUP wheel)
	endif()

	# Ubuntu
	# Default 13.04 install has lsb_release, check the distro name
	if (${LINUX_DISTRO_NAME} MATCHES "Ubuntu")
		set (BLADERF_GROUP plugdev)
	endif()

	message(STATUS
		"${CMAKE_SYSTEM_NAME} ${LINUX_DISTRO_NAME} detected"
		)

	message(STATUS
		"bladeRF will be accessible by group ${BLADERF_GROUP}"
		)

        configure_file(88-nuand.rules.in 88-nuand.rules)
        install(FILES 88-nuand.rules
                DESTINATION /etc/udev/rules.d
                COMPONENT "udev_rules")
    else()
        message(STATUS
                "nuand bladeRF udev rules will not be installed because INSTALL_UDEV_RULES=OFF"
               )
    endif(INSTALL_UDEV_RULES)
endif()
