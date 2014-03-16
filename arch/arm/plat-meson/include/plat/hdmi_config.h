#ifndef _HDMI_CONFIG_H_
#define _HDMI_CONFIG_H_

struct hdmi_phy_set_data{
    unsigned long freq;
    unsigned long addr;
    unsigned long data;
};

struct vendor_info_data{
    unsigned char *vendor_name;     // Max Chars: 8
    unsigned int  vendor_id;        // 3 Bytes, Refer to http://standards.ieee.org/develop/regauth/oui/oui.txt
    unsigned char *product_desc;    // Max Chars: 16
    unsigned char *cec_osd_string;  // Max Chars: 14
};

struct hdmi_config_platform_data{
    void (*hdmi_5v_ctrl)(unsigned int pwr);
    void (*hdmi_3v3_ctrl)(unsigned int pwr);
    void (*hdmi_pll_vdd_ctrl)(unsigned int pwr);
    void (*hdmi_sspll_ctrl)(unsigned int level);    // SSPLL control level
    struct hdmi_phy_set_data *phy_data;             // For some boards, HDMI PHY setting may diff from ref board.
    struct vendor_info_data *vend_data;
};

#endif

