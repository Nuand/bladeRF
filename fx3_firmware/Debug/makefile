################################################################################
# Automatically-generated file. Do not edit!
################################################################################

-include ../makefile.init

RM := cs-rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 
SECONDARY_LIST += \
bladeRF.lst \

SECONDARY_SIZE += \
bladeRF.siz \


# All Target
all: bladeRF.elf secondary-outputs

# Tool invocations
bladeRF.elf: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: ARM Sourcery Windows GCC C Linker'
	arm-none-eabi-gcc $(OBJS) $(USER_OBJS) $(LIBS) -T"C:\Cypress\EZ-USB FX3 SDK\1.2\/firmware/common/fx3.ld" -nostartfiles -Wl,-Map,bladeRF.map -Wl,-d -Wl,--no-wchar-size-warning -Wl,--entry,CyU3PFirmwareEntry "C:\Cypress\EZ-USB FX3 SDK\1.2\\firmware\u3p_firmware\lib\fx3_debug\cyfxapi.a" "C:\Cypress\EZ-USB FX3 SDK\1.2\\firmware\u3p_firmware\lib\fx3_debug\cyu3lpp.a" "C:\Cypress\EZ-USB FX3 SDK\1.2\\firmware\u3p_firmware\lib\fx3_debug\cyu3threadx.a"  "C:\Cypress\EZ-USB FX3 SDK\1.2\\arm-none-eabi\lib\libc.a" "C:\Cypress\EZ-USB FX3 SDK\1.2\\lib\gcc\arm-none-eabi\4.5.2\libgcc.a" -mcpu=arm926ej-s -mthumb-interwork -g -gdwarf-2 -o"bladeRF.elf"
	@echo 'Finished building target: $@'
	@echo ' '
	$(MAKE) --no-print-directory post-build

bladeRF.lst: bladeRF.elf
	@echo 'Invoking: ARM Sourcery Windows GNU Create Listing'
	arm-none-eabi-objdump -h -S bladeRF.elf >"bladeRF.lst"
	@echo 'Finished building: $@'
	@echo ' '

bladeRF.siz: bladeRF.elf
	@echo 'Invoking: ARM Sourcery Windows GNU Print Size'
	arm-none-eabi-size  --format=berkeley bladeRF.elf
	@echo 'Finished building: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(SECONDARY_SIZE)$(OBJS)$(C_DEPS)$(ASM_DEPS)$(EXECUTABLES)$(SECONDARY_LIST)$(S_UPPER_DEPS) bladeRF.elf
	-@echo ' '

post-build:
	-@echo 'Converting ELF to image (.img) format'
	-"C:\Cypress\EZ-USB FX3 SDK\1.2\\util\elf2img\elf2img.exe" -i bladeRF.elf -o bladeRF.img
	-@echo ' '

secondary-outputs: $(SECONDARY_LIST) $(SECONDARY_SIZE)

.PHONY: all clean dependents
.SECONDARY: post-build

-include ../makefile.targets
