#
export ATH_DRIVER_TOPDIR=$(srctree)/drivers/amlogic/wifi/atheros_ath6kl

#################################################################################################################################
# Compat related flags
export
COMPAT_LATEST_VERSION = 1
#KERNEL_VERSION := $(shell make -C $(KERNELPATH) kernelversion | sed -n 's/^\([0-9]\)\..*/\1/p')
#ifneq ($(KERNEL_VERSION),2)
#KERNEL_SUBLEVEL := $(shell make -C $(KERNELPATH) kernelversion | sed -n 's/^3\.\([0-9]\+\).*/\1/p')
#else
#COMPAT_26LATEST_VERSION = 39
#KERNEL_26SUBLEVEL := $(shell make -C $(KERNELPATH) kernelversion | sed -n 's/^2\.6\.\([0-9]\+\).*/\1/p')
#COMPAT_26VERSIONS := $(shell I=$(COMPAT_26LATEST_VERSION); while [ "$$I" -gt $(KERNEL_26SUBLEVEL) ]; do echo $$I; I=$$(($$I - 1)); done)
#$(foreach ver,$(COMPAT_26VERSIONS),$(eval CONFIG_COMPAT_KERNEL_2_6_$(ver)=y))
#KERNEL_SUBLEVEL := -1
#endif
#COMPAT_VERSIONS := $(shell I=$(COMPAT_LATEST_VERSION); while [ "$$I" -gt $(KERNEL_SUBLEVEL) ]; do echo $$I; I=$$(($$I - 1)); done)
#$(foreach ver,$(COMPAT_VERSIONS),$(eval CONFIG_COMPAT_KERNEL_3_$(ver)=y))
#
# Need to build compat_firmware_class.ko ?
#
#include $(srctree)/.config
ifdef CONFIG_COMPAT_KERNEL_2_6_33
ifdef CONFIG_FW_LOADER
 CONFIG_COMPAT_FIRMWARE_CLASS=m
endif
endif

#################################################################################################################################
# CFG80211 related flags

# Default always ON even kernel config don't support it.
ifndef CONFIG_CFG80211
CONFIG_CFG80211=m
DEF_CFG80211=y
endif

ifndef CONFIG_CFG80211_DEFAULT_PS
CONFIG_CFG80211_DEFAULT_PS=y
DEF_CFG80211_DEFAULT_PS=y
endif

ifndef CONFIG_CFG80211_REG_DEBUG
CONFIG_CFG80211_REG_DEBUG=y
DEF_CFG80211_REG_DEBUG=y
endif

# We need CONFIG_WIRELESS_EXT for CONFIG_CFG80211_WEXT for every kernel 
# version. The new way CONFIG_CFG80211_WEXT is called from the kernel 
# does not work with compat-wireless because it calls some callback 
# function on struct wiphy. This struct is shipped with compat-wireless 
# and changes from kernel version to version. We are using the 
# wireless_handlers attribute which will be activated by 
ifdef CONFIG_WIRELESS_EXT
ifndef CONFIG_CFG80211_WEXT
CONFIG_CFG80211_WEXT=y
DEF_CFG80211_WEXT=y
endif
endif

ifndef CONFIG_NL80211_TESTMODE
CONFIG_NL80211_TESTMODE=y
endif

#################################################################################################################################
# ATH6KL related flags
ATH_LOCAL_CFLAGS += -DCONFIG_ATH6KL_DEBUG=y

#################################################################################################################################
# Add flags to compiler
ifdef DEF_CFG80211
ATH_LOCAL_CFLAGS += -DCONFIG_CFG80211=m
endif

ifdef DEF_CFG80211_DEFAULT_PS
ATH_LOCAL_CFLAGS += -DCONFIG_CFG80211_DEFAULT_PS=y
endif

ifdef DEF_CFG80211_REG_DEBUG
ATH_LOCAL_CFLAGS += -DCONFIG_CFG80211_REG_DEBUG=y
endif

ifdef DEF_CFG80211_WEXT
ATH_LOCAL_CFLAGS += -DCONFIG_CFG80211_WEXT=y
endif

ifdef DEF_CFG80211_INTERNAL_REGDB
CONFIG_CFG80211_INTERNAL_REGDB=y
ATH_LOCAL_CFLAGS += -DCONFIG_CFG80211_INTERNAL_REGDB=y
endif

ifdef CONFIG_NL80211_TESTMODE
ATH_LOCAL_CFLAGS += -DCONFIG_NL80211_TESTMODE=y
endif

NOSTDINC_FLAGS := -I$(srctree)/drivers/amlogic/wifi/atheros_ath6kl/include/ \
	-include $(srctree)/drivers/amlogic/wifi/atheros_ath6kl/include/linux/compat-2.6.h\
	$(CFLAGS)

#################################################################################################################################
all: driver_clean driver_build

obj-$(CONFIG_ATH6KL_USB_WIFI)	:= compat/
obj-$(CONFIG_ATH6KL_USB_WIFI)	+= cfg80211/
obj-$(CONFIG_ATH6KL_USB_WIFI)	+= ath6kl/


clean: driver_clean

driver_build:
	@echo Build drivers
	${MAKEARCH} OBJ_ROOT=$(ATH_DRIVER_TOPDIR)/compat -C $(KERNELPATH) SUBDIRS=$(ATH_DRIVER_TOPDIR)/compat modules
	${MAKEARCH} OBJ_ROOT=$(ATH_DRIVER_TOPDIR)/cfg80211 -C $(KERNELPATH) SUBDIRS=$(ATH_DRIVER_TOPDIR)/cfg80211 KBUILD_EXTRA_SYMBOLS=$(ATH_DRIVER_TOPDIR)/compat/Module.symvers modules
	${MAKEARCH} OBJ_ROOT=$(ATH_DRIVER_TOPDIR)/ath6kl -C $(KERNELPATH) SUBDIRS=$(ATH_DRIVER_TOPDIR)/ath6kl KBUILD_EXTRA_SYMBOLS=$(ATH_DRIVER_TOPDIR)/cfg80211/Module.symvers modules

driver_clean:
	@echo Clean drivers
	${MAKEARCH} -C $(KERNELPATH) SUBDIRS=$(ATH_DRIVER_TOPDIR)/compat clean
	${MAKEARCH} -C $(KERNELPATH) SUBDIRS=$(ATH_DRIVER_TOPDIR)/cfg80211 clean
	${MAKEARCH} -C $(KERNELPATH) SUBDIRS=$(ATH_DRIVER_TOPDIR)/ath6kl clean

# Move to BOARD-specific Makefile
#driver_patch:
#	@echo Patch drivers
#	@patch -p0 < patches/00-remore_ns-type.patch
#	@patch -p0 < patches/02-remore_netlink-seq.patch
#	@patch -p0 < patches/03-change_IFF_BRIDGE_PORT.patch

firmware_install:
	@echo install AR6004 REV2 firmware
	@rm -rf ${INSTALL_ROOT}/lib/firmware/ath6k/AR6004/hw1.1/
	@mkdir -p ${INSTALL_ROOT}/lib/firmware/ath6k/AR6004/hw1.1/
	@cp $(ATH_DRIVER_TOPDIR)/fw/firmware/AR6004/hw1.1/bdata.bin  ${INSTALL_ROOT}/lib/firmware/ath6k/AR6004/hw1.1/
	@cp $(ATH_DRIVER_TOPDIR)/fw/firmware/AR6004/hw1.1/fw.ram.bin ${INSTALL_ROOT}/lib/firmware/ath6k/AR6004/hw1.1/
	@if [ "$(BOARD_TYPE)" = "x86" -o "$(BOARD_TYPE)" = "x86-combo" ]; then \
		if [ "$(BUILD_DRIVERFW_TOLOCAL)" = "y" ]; then \
			echo install AR6004 REV2 firmware to local; \
			rm -rf /lib/firmware/ath6k/AR6004/hw1.1/; \
			mkdir -p /lib/firmware/ath6k/AR6004/hw1.1/; \
			cp $(ATH_DRIVER_TOPDIR)/fw/firmware/AR6004/hw1.1/bdata.bin  /lib/firmware/ath6k/AR6004/hw1.1/; \
			cp $(ATH_DRIVER_TOPDIR)/fw/firmware/AR6004/hw1.1/fw.ram.bin /lib/firmware/ath6k/AR6004/hw1.1/; \
			cp $(ATH_DRIVER_TOPDIR)/fw/firmware/AR6004/hw1.1/fw.ram.tx99.bin /lib/firmware/ath6k/AR6004/hw1.1/; \
		else \
			echo You must be root to install firmware to local...; \
		fi \
	fi

firmware_toarray:
	@echo Transfer firmware to C array
	@${ATH_TOPDIR}/build/util/ath6kl_bin2hex.sh $(ATH_DRIVER_TOPDIR)/fw/firmware/AR6004/hw1.1/bdata.bin  ${ATH_DRIVER_TOPDIR}/ath6kl/fw_bdata.h       fw_bdata
	@echo Transfer $(ATH_DRIVER_TOPDIR)/fw/firmware/AR6004/hw1.1/bdata.bin DONE...
	@${ATH_TOPDIR}/build/util/ath6kl_bin2hex.sh $(ATH_DRIVER_TOPDIR)/fw/firmware/AR6004/hw1.1/fw.ram.bin ${ATH_DRIVER_TOPDIR}/ath6kl/fw_ram.h         fw_ram
	@echo Transfer $(ATH_DRIVER_TOPDIR)/fw/firmware/AR6004/hw1.1/fw.ram.bin DONE...
	@${ATH_TOPDIR}/build/util/ath6kl_bin2hex.sh $(ATH_DRIVER_TOPDIR)/fw/firmware/AR6004/hw1.1/utf.bin ${ATH_DRIVER_TOPDIR}/ath6kl/fw_utf.h         fw_utf
	@echo Transfer $(ATH_DRIVER_TOPDIR)/fw/firmware/AR6004/hw1.1/utf.bin DONE...
	@${ATH_TOPDIR}/build/util/ath6kl_bin2hex.sh $(ATH_DRIVER_TOPDIR)/fw/firmware/AR6004/hw1.1/nullTestFlow.bin ${ATH_DRIVER_TOPDIR}/ath6kl/fw_nullTestFlow.h         fw_nullTestFlow
	@echo Transfer $(ATH_DRIVER_TOPDIR)/fw/firmware/AR6004/hw1.1/nullTestFlow.bin DONE...
