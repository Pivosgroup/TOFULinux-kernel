#include <linux/utsname.h>
#include <net/cfg80211.h>
#include "core.h"
#include "ethtool.h"

static void cfg80211_get_drvinfo(struct net_device *dev,
					struct ethtool_drvinfo *info)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;

	strlcpy(info->driver, wiphy_dev(wdev->wiphy)->driver->name,
		sizeof(info->driver));

	strlcpy(info->version, init_utsname()->release, sizeof(info->version));

	if (wdev->wiphy->fw_version[0])
		strncpy(info->fw_version, wdev->wiphy->fw_version,
			sizeof(info->fw_version));
	else
		strncpy(info->fw_version, "N/A", sizeof(info->fw_version));

	strlcpy(info->bus_info, dev_name(wiphy_dev(wdev->wiphy)),
		sizeof(info->bus_info));
}

static int cfg80211_get_regs_len(struct net_device *dev)
{
	/* For now, return 0... */
	return 0;
}

static void cfg80211_get_regs(struct net_device *dev, struct ethtool_regs *regs,
			void *data)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;

	regs->version = wdev->wiphy->hw_version;
	regs->len = 0;
}

static void cfg80211_get_ringparam(struct net_device *dev,
				   struct ethtool_ringparam *rp)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct cfg80211_registered_device *rdev = wiphy_to_dev(wdev->wiphy);

	memset(rp, 0, sizeof(*rp));

	if (rdev->ops->get_ringparam)
		rdev->ops->get_ringparam(wdev->wiphy,
					 &rp->tx_pending, &rp->tx_max_pending,
					 &rp->rx_pending, &rp->rx_max_pending);
}

static int cfg80211_set_ringparam(struct net_device *dev,
				  struct ethtool_ringparam *rp)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct cfg80211_registered_device *rdev = wiphy_to_dev(wdev->wiphy);

	if (rp->rx_mini_pending != 0 || rp->rx_jumbo_pending != 0)
		return -EINVAL;

	if (rdev->ops->set_ringparam)
		return rdev->ops->set_ringparam(wdev->wiphy,
						rp->tx_pending, rp->rx_pending);

	return -ENOTSUPP;
}

static void    cfg80211_get_wol(struct net_device *dev, struct ethtool_wolinfo * wolinfo) 
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct cfg80211_registered_device *rdev = wiphy_to_dev(wdev->wiphy);
	struct cfg80211_wowlan *ntrig;
	ntrig = rdev->wowlan;

	wolinfo->supported = WAKE_MAGIC | WAKE_MAGICSECURE;
	wolinfo->wolopts = 0;
	memset(&wolinfo->sopass, 0, sizeof(wolinfo->sopass));
	
	if(ntrig != NULL) {
		if(ntrig->magic_pkt)
			wolinfo->wolopts |= WAKE_MAGIC;
		if(ntrig->n_patterns == 1) {
			if(ntrig->patterns[0].pattern_len == 6) {
				wolinfo->wolopts |= WAKE_MAGICSECURE;		
				memcpy(wolinfo->sopass,ntrig->patterns[0].pattern,6);
			}
		}
	}
}

static int     cfg80211_set_wol(struct net_device *dev, struct ethtool_wolinfo *wolinfo)
{
	struct wireless_dev *wdev = dev->ieee80211_ptr;
	struct cfg80211_registered_device *rdev = wiphy_to_dev(wdev->wiphy);
	struct cfg80211_wowlan *ntrig;
	struct cfg80211_wowlan no_triggers = {};
	struct cfg80211_wowlan new_triggers = {};
	int err=0,i;
	
	memset(&new_triggers,0x00,sizeof(new_triggers));

	if((wolinfo->wolopts & WAKE_MAGIC) == WAKE_MAGIC) {
		new_triggers.disconnect = true;	
		new_triggers.magic_pkt = true;
		new_triggers.gtk_rekey_failure = true;		
	}
	if((wolinfo->wolopts & WAKE_MAGICSECURE) == WAKE_MAGICSECURE) {
		u8 pattern_mask = 0x3f;
		new_triggers.n_patterns = 1;
		new_triggers.patterns = kcalloc(new_triggers.n_patterns,
						sizeof(new_triggers.patterns[0]),
						GFP_KERNEL);
		if (!new_triggers.patterns)
			return -ENOMEM;

		new_triggers.patterns[0].mask =
				kmalloc(1 + 6, GFP_KERNEL);
		if (!new_triggers.patterns[0].mask) {
			err = -ENOMEM;
			goto error;
		}
	
		new_triggers.patterns[0].pattern =
			new_triggers.patterns[0].mask + 1;
		memcpy(new_triggers.patterns[0].mask ,&pattern_mask,1);
		new_triggers.patterns[0].pattern_len = 6;
	
		memcpy(new_triggers.patterns[0].pattern,
			   wolinfo->sopass,
			   6);
	}

	if (memcmp(&new_triggers, &no_triggers, sizeof(new_triggers))) {
		struct cfg80211_wowlan *ntrig;
		ntrig = kmemdup(&new_triggers, sizeof(new_triggers),
				GFP_KERNEL);
		if (!ntrig) {
			err = -ENOMEM;
			goto error;
		}	
		cfg80211_rdev_free_wowlan(rdev);
		rdev->wowlan = ntrig;

		if(rdev->ops->set_wow_mode) {		
			rdev->ops->set_wow_mode(&rdev->wiphy, ntrig);
		}
	}
	return 0;
error:	
	for (i = 0; i < new_triggers.n_patterns; i++)
		kfree(new_triggers.patterns[i].mask);
	kfree(new_triggers.patterns);
	return err;
}

const struct ethtool_ops cfg80211_ethtool_ops = {
	.get_drvinfo = cfg80211_get_drvinfo,
	.get_regs_len = cfg80211_get_regs_len,
	.get_regs = cfg80211_get_regs,
	.get_link = ethtool_op_get_link,
	.get_ringparam = cfg80211_get_ringparam,
	.set_ringparam = cfg80211_set_ringparam,
    .get_wol = cfg80211_get_wol,
    .set_wol = cfg80211_set_wol,
};
