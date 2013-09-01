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
        install(FILES init_board.sh version
                DESTINATION share/bladerf/scripts
                COMPONENT "udev_rules")
        install(FILES bladerf.cfg
                DESTINATION /etc
                COMPONENT "udev_rules")
    else()
        message(STATUS
                "nuand bladeRF udev rules will not be installed because INSTALL_UDEV_RULES=OFF"
               )
    endif(INSTALL_UDEV_RULES)
endif()
