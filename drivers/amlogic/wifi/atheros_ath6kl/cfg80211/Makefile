NOSTDINC_FLAGS := -include ${ATH_DRIVER_TOPDIR}/include/linux/compat-2.6.h -I${ATH_DRIVER_TOPDIR}/include/ $(ATH_LOCAL_CFLAGS)
obj-$(CONFIG_CFG80211) += cfg80211_ath6kl.o


cfg80211_ath6kl-y += core.o sysfs.o radiotap.o util.o reg.o scan.o nl80211.o
cfg80211_ath6kl-y += mlme.o ibss.o sme.o chan.o ethtool.o mesh.o
cfg80211_ath6kl-$(CONFIG_CFG80211_DEBUGFS) += debugfs.o
cfg80211_ath6kl-$(CONFIG_CFG80211_WEXT) += wext-compat.o wext-sme.o
cfg80211_ath6kl-$(CONFIG_CFG80211_INTERNAL_REGDB) += regdb.o

ccflags-y += -D__CHECK_ENDIAN__


