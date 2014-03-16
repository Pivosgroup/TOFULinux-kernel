#
# Makefile for sound control interface
#

# Toplevel Module Dependency
amlogic_amaudio-objs := amaudio.o


ifndef CONFIG_MESON_TRUSTZONE
ifneq ($(KBUILD_SRC),)
TOP_KBUILD_SRC := $(KBUILD_SRC)/
endif
$(obj)/amaud.o:
	@cp $(TOP_KBUILD_SRC)drivers/amlogic/amaudio/amaudio.z \
        drivers/amlogic/amaudio/amaud.o
amlogic_amaudio-objs += amaud.o
endif

obj-$(CONFIG_AMAUDIO) += amlogic_amaudio.o