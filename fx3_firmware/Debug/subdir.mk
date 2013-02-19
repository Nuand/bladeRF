################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../bladeRF.c \
../cyfxbladeRFusbdscr.c \
../cyfxtx.c \
../spi_flash_lib.c 

S_UPPER_SRCS += \
../cyfx_gcc_startup.S 

OBJS += \
./bladeRF.o \
./cyfx_gcc_startup.o \
./cyfxbladeRFusbdscr.o \
./cyfxtx.o \
./spi_flash_lib.o 

C_DEPS += \
./bladeRF.d \
./cyfxbladeRFusbdscr.d \
./cyfxtx.d \
./spi_flash_lib.d 

S_UPPER_DEPS += \
./cyfx_gcc_startup.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Sourcery Windows GCC C Compiler'
	arm-none-eabi-gcc -D__CYU3P_TX__=1 -I"C:\Cypress\EZ-USB FX3 SDK\1.2\\firmware\u3p_firmware\inc" -O0 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -mcpu=arm926ej-s -mthumb-interwork -g -gdwarf-2 -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

%.o: ../%.S
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Sourcery Windows GCC Assembler'
	arm-none-eabi-gcc -x assembler-with-cpp -I"C:\Cypress\EZ-USB FX3 SDK\1.2\\firmware\u3p_firmware\inc" -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@:%.o=%.d)" -mcpu=arm926ej-s -mthumb-interwork -g -gdwarf-2 -o"$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


