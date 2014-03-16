#
# eb secure

ifneq ($(KBUILD_SRC),)
TOP_KBUILD_SRC := $(KBUILD_SRC)/
endif

CONFIG_EB_SECURE_SRC:=
$(obj)/m6_ebsecure.o: FORCE
	@cp $(TOP_KBUILD_SRC)drivers/amlogic/ebsecure/m6_ebsecure.z \
    drivers/amlogic/ebsecure/m6_ebsecure.o

ifeq ($(CONFIG_EB_SECURE_SRC),)
ebsecure-objs += m6_ebsecure.o
obj-$(CONFIG_EB_SECURE) += ebsecure.o
else

endif

