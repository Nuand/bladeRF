FX3FWROOT="C:/Cypress/EZ-USB FX3 SDK/1.2/firmware"
FX3PFWROOT="C:/Cypress/EZ-USB FX3 SDK/1.2/firmware/u3p_firmware"

all:compile

include $(FX3FWROOT)/common/fx3_build_config.mak

MODULE = bladeRF


SOURCE += $(MODULE).c

C_OBJECT=$(SOURCE:%.c=./%.o)
A_OBJECT=$(SOURCE_ASM:%.S=./%.o)

EXES = $(MODULE).$(EXEEXT)

$(MODULE).$(EXEEXT): $(A_OBJECT) $(C_OBJECT)
	$(LINK)

$(C_OBJECT) : %.o : %.c
	$(COMPILE)

$(A_OBJECT) : %.o : %.S
	$(ASSEMBLE)

clean:
	rm -f ./$(MODULE).$(EXEEXT)
	rm -f ./$(MODULE).map
	rm -f ./*.o

compile: $(C_OBJECT) $(A_OBJECT) $(EXES)

#[]#
