#lib-y = Common.o HostComm.o Main.o MemoryMapChipOne.o PeripheralsDriverLib.o TouchCommon.o TouchDataProcessLib.o WorkModeNormal.o

ifneq ($(KBUILD_SRC),)
TOP_KBUILD_SRC := $(KBUILD_SRC)/
endif

MAKE_FOR_LIB := n
ifeq ($(MAKE_FOR_LIB), n)
$(obj)/icn838x_algorithm_lib:
	@cp $(TOP_KBUILD_SRC)drivers/amlogic/input/touchscreen/icn838x/lib.z \
        drivers/amlogic/input/touchscreen/icn838x/icn838x_algorithm_lib.o 
endif

ifeq ($(MAKE_FOR_LIB), y)
    icn838x_algorithm_lib-y := icn838x_algorithm.o
else 
    # add FORCE to make compile force copy obj file, so no need to delete obj file
    # when .z has updated
    $(obj)/icn838x_algorithm_lib.o: $(obj)/icn838x_algorithm_lib FORCE
endif

obj-$(CONFIG_ICN838x_CAPACITIVE_TOUCHSCREEN) += icn838x.o
icn838x-objs += icn838x_iic_ts.o icn838x_iic_slave.o
icn838x-objs += icn838x_algorithm_lib.o
