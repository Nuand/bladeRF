cmake_minimum_required(VERSION 3.10...3.27)

################################################################################
# Install libbladeRF header files
################################################################################
install(FILES
        libbladeRF.h
        bladeRF1.h
        bladeRF2.h
        DESTINATION include
       )

