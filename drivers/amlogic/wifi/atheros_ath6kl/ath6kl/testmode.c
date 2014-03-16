/*
 * Copyright (c) 2010-2011 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "testmode.h"

#include <net/netlink.h>

enum ath6kl_tm_attr {
	__ATH6KL_TM_ATTR_INVALID	= 0,
	ATH6KL_TM_ATTR_CMD		= 1,
	ATH6KL_TM_ATTR_DATA		= 2,

	/* keep last */
	__ATH6KL_TM_ATTR_AFTER_LAST,
	ATH6KL_TM_ATTR_MAX		= __ATH6KL_TM_ATTR_AFTER_LAST - 1,
};

enum ath6kl_tm_cmd {
	ATH6KL_TM_CMD_TCMD		= 0,
};

#define ATH6KL_TM_DATA_MAX_LEN		5000

static const struct nla_policy ath6kl_tm_policy[ATH6KL_TM_ATTR_MAX + 1] = {
	[ATH6KL_TM_ATTR_CMD]		= { .type = NLA_U32 },
	[ATH6KL_TM_ATTR_DATA]		= { .type = NLA_BINARY,
					    .len = ATH6KL_TM_DATA_MAX_LEN },
};

void ath6kl_tm_rx_report_event(struct ath6kl *ar, void *buf, size_t buf_len)
{
	if (down_interruptible(&ar->sem))
		return;

	kfree(ar->tm.rx_report);

	ar->tm.rx_report = kmemdup(buf, buf_len, GFP_KERNEL);
	ar->tm.rx_report_len = buf_len;

	up(&ar->sem);

	wake_up(&ar->event_wq);
}

extern struct sk_buff *cfg80211_testmode_alloc_event_skb(struct wiphy *wiphy,
                                                  int approxlen, gfp_t gfp);
extern void cfg80211_testmode_event(struct sk_buff *skb, gfp_t gfp);

void ath6kl_tm_rx_event(struct ath6kl *ar, void *buf, size_t buf_len)
{
	struct sk_buff *skb;
    u32 test_cmd_id, act;
    u32 *datap;
    struct ath6kl_tcmd_cont_rx *tcmd_cont_rx;
    int i;

        if (!buf || buf_len == 0)
        {
	    printk(KERN_ERR "buf buflen is empty\n");
	    return;
        }
 
	skb = cfg80211_testmode_alloc_event_skb(ar->wiphy, buf_len, GFP_ATOMIC);

	if (!skb) {
		printk (KERN_ERR "failed to allocate testmode rx skb!\n");
		return;
        }

    datap = (u32 *)buf;
    test_cmd_id = le32_to_cpu(datap[0]);
    act = le32_to_cpu(datap[1]);

    if (test_cmd_id == TCMD_CONT_RX_ID && act == TCMD_CONT_RX_REPORT) {
        tcmd_cont_rx = (struct ath6kl_tcmd_cont_rx *)buf;

        tcmd_cont_rx->test_cmd_id = le32_to_cpu(tcmd_cont_rx->test_cmd_id);
        tcmd_cont_rx->act = le32_to_cpu(tcmd_cont_rx->act);
        tcmd_cont_rx->en_ani = le32_to_cpu(tcmd_cont_rx->en_ani);
        tcmd_cont_rx->u.report.total_pkt = le32_to_cpu(tcmd_cont_rx->u.report.total_pkt);
        tcmd_cont_rx->u.report.rssi_in_dbm = le32_to_cpu(tcmd_cont_rx->u.report.rssi_in_dbm);
        tcmd_cont_rx->u.report.crc_err_pkt = le32_to_cpu(tcmd_cont_rx->u.report.crc_err_pkt);
        tcmd_cont_rx->u.report.sec_err_pkt = le32_to_cpu(tcmd_cont_rx->u.report.sec_err_pkt);
        for (i=0; i<TCMD_MAX_RATES; i++) {
            tcmd_cont_rx->u.report.rate_cnt[i] = le16_to_cpu(tcmd_cont_rx->u.report.rate_cnt[i]);
            tcmd_cont_rx->u.report.rate_cnt_short_guard[i] = le16_to_cpu(tcmd_cont_rx->u.report.rate_cnt_short_guard[i]);
        }
    }

	NLA_PUT_U32(skb, ATH6KL_TM_ATTR_CMD, ATH6KL_TM_CMD_TCMD);
	NLA_PUT(skb, ATH6KL_TM_ATTR_DATA, buf_len, buf);
	cfg80211_testmode_event(skb, GFP_ATOMIC);
	return;
 
 nla_put_failure:
	kfree_skb(skb);
	printk(KERN_ERR "nla_put failed on testmode rx skb!\n");
}

static int ath6kl_tm_rx_report(struct ath6kl *ar, void *buf, size_t buf_len,
			       struct sk_buff *skb)
{
	int ret = 0;
	long left;

	if (!test_bit(WMI_READY, &ar->flag)) {
		ret = -EIO;
		goto out;
	}

	if (test_bit(DESTROY_IN_PROGRESS, &ar->flag)) {
		ret = -EBUSY;
		goto out;
	}
	if (down_interruptible(&ar->sem))
		return -EIO;

	if (ath6kl_wmi_test_cmd(ar->wmi, buf, buf_len) < 0) {
		up(&ar->sem);
		return -EIO;
	}

	left = wait_event_interruptible_timeout(ar->event_wq,
						ar->tm.rx_report != NULL,
						WMI_TIMEOUT);

	if (left == 0) {
		ret = -ETIMEDOUT;
		goto out;
	} else if (left < 0) {
		ret = left;
		goto out;
	}

	if (ar->tm.rx_report == NULL || ar->tm.rx_report_len == 0) {
		ret = -EINVAL;
		goto out;
	}

	NLA_PUT(skb, ATH6KL_TM_ATTR_DATA, ar->tm.rx_report_len,
		ar->tm.rx_report);

	kfree(ar->tm.rx_report);
	ar->tm.rx_report = NULL;

out:
	up(&ar->sem);

	return ret;

nla_put_failure:
	ret = -ENOBUFS;
	goto out;
}
EXPORT_SYMBOL(ath6kl_tm_rx_report);

int ath6kl_tm_cmd(struct wiphy *wiphy, void *data, int len)
{
	struct ath6kl *ar = wiphy_priv(wiphy);
	struct nlattr *tb[ATH6KL_TM_ATTR_MAX + 1];
        int err, buf_len;
	void *buf;
    u32 test_cmd_id, act;
    u32 *datap;
    struct ath6kl_tcmd_cont_tx *tcmd_cont_tx;
    struct ath6kl_tcmd_cont_rx *tcmd_cont_rx;
    struct ath6kl_tcmd_pm *tcmd_pm;
    struct ath6kl_tcmd_set_reg *tcmd_set_reg;
    struct ath6kl_tc_cmds *tc_cmds;

	err = nla_parse(tb, ATH6KL_TM_ATTR_MAX, data, len,
			ath6kl_tm_policy);
         
	if (err)
		return err;

	if (!tb[ATH6KL_TM_ATTR_CMD])
		return -EINVAL;

	switch (nla_get_u32(tb[ATH6KL_TM_ATTR_CMD])) {
	case ATH6KL_TM_CMD_TCMD:
		if (!tb[ATH6KL_TM_ATTR_DATA])
			return -EINVAL;

		buf = nla_data(tb[ATH6KL_TM_ATTR_DATA]);
		buf_len = nla_len(tb[ATH6KL_TM_ATTR_DATA]);

        datap = (u32 *)buf;
        test_cmd_id = datap[0];
        switch (test_cmd_id) {
        case TCMD_CONT_TX_ID:
            tcmd_cont_tx = (struct ath6kl_tcmd_cont_tx *)buf;

            tcmd_cont_tx->test_cmd_id = cpu_to_le32(tcmd_cont_tx->test_cmd_id);
            tcmd_cont_tx->mode = cpu_to_le32(tcmd_cont_tx->mode);
            tcmd_cont_tx->freq = cpu_to_le32(tcmd_cont_tx->freq);
            tcmd_cont_tx->data_rate = cpu_to_le32(tcmd_cont_tx->data_rate);
            tcmd_cont_tx->tx_pwr = cpu_to_le32(tcmd_cont_tx->tx_pwr);
            tcmd_cont_tx->antenna = cpu_to_le32(tcmd_cont_tx->antenna);
            tcmd_cont_tx->en_ani = cpu_to_le32(tcmd_cont_tx->en_ani);
            tcmd_cont_tx->scrambler_off = cpu_to_le32(tcmd_cont_tx->scrambler_off);
            tcmd_cont_tx->aifsn = cpu_to_le32(tcmd_cont_tx->aifsn);
            tcmd_cont_tx->pkt_sz = cpu_to_le16(tcmd_cont_tx->pkt_sz);
            tcmd_cont_tx->tx_pattern = cpu_to_le16(tcmd_cont_tx->tx_pattern);
            tcmd_cont_tx->short_guard = cpu_to_le32(tcmd_cont_tx->short_guard);
            tcmd_cont_tx->num_packets = cpu_to_le32(tcmd_cont_tx->num_packets);
            tcmd_cont_tx->wlan_mode = cpu_to_le32(tcmd_cont_tx->wlan_mode);
            tcmd_cont_tx->lpreamble = cpu_to_le32(tcmd_cont_tx->lpreamble);
            tcmd_cont_tx->tx_chain = cpu_to_le32(tcmd_cont_tx->tx_chain);
            tcmd_cont_tx->misc_flags = cpu_to_le32(tcmd_cont_tx->misc_flags);
            tcmd_cont_tx->broadcast = cpu_to_le32(tcmd_cont_tx->broadcast);
            tcmd_cont_tx->bandwidth = cpu_to_le16(tcmd_cont_tx->bandwidth);
            tcmd_cont_tx->un_used2 = cpu_to_le16(tcmd_cont_tx->un_used2);
            tcmd_cont_tx->un_used3 = cpu_to_le16(tcmd_cont_tx->un_used3);
            tcmd_cont_tx->tpcm = cpu_to_le32(tcmd_cont_tx->tpcm);
            tcmd_cont_tx->retries = cpu_to_le32(tcmd_cont_tx->retries);
            tcmd_cont_tx->agg = cpu_to_le32(tcmd_cont_tx->agg);
            tcmd_cont_tx->n_pattern = cpu_to_le32(tcmd_cont_tx->n_pattern);

            break;
        case TCMD_CONT_RX_ID:
            tcmd_cont_rx = (struct ath6kl_tcmd_cont_rx *)buf;
            act = tcmd_cont_rx->act;

            tcmd_cont_rx->test_cmd_id = cpu_to_le32(tcmd_cont_rx->test_cmd_id);
            tcmd_cont_rx->act = cpu_to_le32(tcmd_cont_rx->act);
            tcmd_cont_rx->en_ani = cpu_to_le32(tcmd_cont_rx->en_ani);
            switch (act) {
            case TCMD_CONT_RX_PROMIS:
            case TCMD_CONT_RX_FILTER:
            case TCMD_CONT_RX_REPORT:
                tcmd_cont_rx->u.para.freq = cpu_to_le32(tcmd_cont_rx->u.para.freq);
                tcmd_cont_rx->u.para.antenna = cpu_to_le32(tcmd_cont_rx->u.para.antenna);
                tcmd_cont_rx->u.para.wlan_mode = cpu_to_le32(tcmd_cont_rx->u.para.wlan_mode);
                tcmd_cont_rx->u.para.ack = cpu_to_le32(tcmd_cont_rx->u.para.ack);
                tcmd_cont_rx->u.para.rx_chain = cpu_to_le32(tcmd_cont_rx->u.para.rx_chain);
                tcmd_cont_rx->u.para.bc = cpu_to_le32(tcmd_cont_rx->u.para.bc);
                tcmd_cont_rx->u.para.bandwidth = cpu_to_le32(tcmd_cont_rx->u.para.bandwidth);
                tcmd_cont_rx->u.para.lpl = cpu_to_le32(tcmd_cont_rx->u.para.lpl);
                break;
            case TCMD_CONT_RX_SETMAC:
                tcmd_cont_rx->u.mac.otp_write_flag = cpu_to_le32(tcmd_cont_rx->u.mac.otp_write_flag);
                break;
            case TCMD_CONT_RX_SET_ANT_SWITCH_TABLE:
                tcmd_cont_rx->u.antswitchtable.antswitch1 = cpu_to_le32(tcmd_cont_rx->u.antswitchtable.antswitch1);
                tcmd_cont_rx->u.antswitchtable.antswitch2 = cpu_to_le32(tcmd_cont_rx->u.antswitchtable.antswitch2);
                break;
            default:
                return -EOPNOTSUPP;
            }
            break;
        case TCMD_PM_ID:
            tcmd_pm = (struct ath6kl_tcmd_pm *)buf;

            tcmd_pm->test_cmd_id = cpu_to_le32(tcmd_pm->test_cmd_id);
            tcmd_pm->mode = cpu_to_le32(tcmd_pm->mode);
            break;
        case TC_CMDS_ID:
            tc_cmds = (struct ath6kl_tc_cmds *)buf;

            tc_cmds->hdr.test_cmd_id = cpu_to_le32(tc_cmds->hdr.test_cmd_id);
            tc_cmds->hdr.act = cpu_to_le32(tc_cmds->hdr.act);
            tc_cmds->hdr.u.parm.length = cpu_to_le16(tc_cmds->hdr.u.parm.length);
            break;
        case TCMD_SET_REG_ID:
            tcmd_set_reg = (struct ath6kl_tcmd_set_reg *)buf;

            tcmd_set_reg->test_cmd_id = cpu_to_le32(tcmd_set_reg->test_cmd_id);
            tcmd_set_reg->reg_addr = cpu_to_le32(tcmd_set_reg->reg_addr);
            tcmd_set_reg->val = cpu_to_le32(tcmd_set_reg->val);
            tcmd_set_reg->flag = cpu_to_le16(tcmd_set_reg->flag);
            break;

        default:
            return -EOPNOTSUPP;
        }

		ath6kl_wmi_test_cmd(ar->wmi, buf, buf_len);

		return 0;

		break;
	default:
		return -EOPNOTSUPP;
	}
}
