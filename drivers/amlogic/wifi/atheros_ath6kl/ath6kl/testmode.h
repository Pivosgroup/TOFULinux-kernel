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

#include "core.h"

#ifdef CONFIG_NL80211_TESTMODE

void ath6kl_tm_rx_report_event(struct ath6kl *ar, void *buf, size_t buf_len);
void ath6kl_tm_rx_event(struct ath6kl *ar, void *buf, size_t buf_len);
int ath6kl_tm_cmd(struct wiphy *wiphy, void *data, int len);

#else

static inline void ath6kl_tm_rx_report_event(struct ath6kl *ar, void *buf, size_t buf_len)
{
}

static inline int ath6kl_tm_cmd(struct wiphy *wiphy, void *data, int len)
{
	return 0;
}

static inline void ath6kl_tm_rx_event(struct ath6kl *ar, void *buf, size_t buf_len)
{
}

#endif

#ifdef AR6002_REV2
#define TCMD_MAX_RATES 12
#else
#define TCMD_MAX_RATES 47
#endif

#define ATH_MAC_LEN             6
#define MPATTERN                (10*4)

#define TC_CMDS_SIZE_MAX  255

struct ath6kl_tcmd_cont_tx {
    __le32 test_cmd_id;
    __le32 mode;
    __le32 freq;
    __le32 data_rate;
    __le32 tx_pwr;
    __le32 antenna;
    __le32 en_ani;
    __le32 scrambler_off;
    __le32 aifsn;
    __le16 pkt_sz;
    __le16 tx_pattern;
    __le32 short_guard;
    __le32 num_packets;
    __le32 wlan_mode;
    __le32 lpreamble;
    __le32 tx_chain;

    __le32 misc_flags;
    __le32 broadcast;
    u8 bssid[ATH_MAC_LEN];
    __le16 bandwidth;
    u8 tx_station[ATH_MAC_LEN];
    __le16 un_used2;
    u8 rx_station[ATH_MAC_LEN];
    __le16 un_used3;
    __le32 tpcm;
    __le32 retries;
    __le32 agg;
    __le32 n_pattern;
    u8 data_pattern[MPATTERN]; // bytes to be written ;
} __packed;

typedef enum {
        TCMD_CONT_RX_PROMIS = 0,
        TCMD_CONT_RX_FILTER,
        TCMD_CONT_RX_REPORT,
        TCMD_CONT_RX_SETMAC,
        TCMD_CONT_RX_SET_ANT_SWITCH_TABLE
} TCMD_CONT_RX_ACT;

struct ath6kl_tcmd_cont_rx {
    __le32 test_cmd_id;
    __le32 act;
    __le32 en_ani;
    union {
        struct {
            __le32 freq;
            __le32 antenna;
            __le32 wlan_mode;
            __le32 ack;
            __le32 rx_chain;
            __le32 bc;
            __le32 bandwidth;
            __le32 lpl;/* low power listen */
        } para;
        struct {
            __le32 total_pkt;
            __le32 rssi_in_dbm;
            __le32 crc_err_pkt;
            __le32 sec_err_pkt;
            __le16 rate_cnt[TCMD_MAX_RATES];
            __le16 rate_cnt_short_guard[TCMD_MAX_RATES];
        } report;
        struct {
            u8 addr[ATH_MAC_LEN];
            u8 bssid[ATH_MAC_LEN];
            u8 btaddr[ATH_MAC_LEN];     
            u8 reg_dmn[2];
            __le32   otp_write_flag;
        } mac;
        struct {
            __le32 antswitch1;
            __le32 antswitch2;
        } antswitchtable;
    } u;
} __packed;

struct ath6kl_tcmd_pm {
    __le32 test_cmd_id;
    __le32 mode;
} __packed;

typedef enum {
    TC_CMDS_TS =0,
    TC_CMDS_CAL,
    TC_CMDS_TPCCAL = TC_CMDS_CAL,
    TC_CMDS_TPCCAL_WITH_OTPWRITE,
    TC_CMDS_OTPDUMP,
    TC_CMDS_OTPSTREAMWRITE,    
    TC_CMDS_EFUSEDUMP,    
    TC_CMDS_EFUSEWRITE,    
    TC_CMDS_READTHERMAL,
    TC_CMDS_SET_BT_MODE,
} TC_CMDS_ACT;

struct ath6kl_tc_cmds_hdr {
    __le32 test_cmd_id;
    __le32 act;
    union {
        __le32 en_ani;    // to be identical to CONT_RX struct
        struct {
            __le16 length;
            u8 version;
            u8 buf_len;
        } parm;
    } u;
} __packed;

struct ath6kl_tc_cmds {
    struct ath6kl_tc_cmds_hdr hdr;
    u8  buf[TC_CMDS_SIZE_MAX+1];
} __packed;

struct ath6kl_tcmd_set_reg {
    __le32 test_cmd_id;
    __le32 reg_addr;
    __le32 val;
    __le16 flag;
} __packed;

typedef enum {
    TCMD_CONT_TX_ID,
    TCMD_CONT_RX_ID,
    TCMD_PM_ID,
    TC_CMDS_ID,
    TCMD_SET_REG_ID,

    INVALID_CMD_ID=255,
} TCMD_ID;

