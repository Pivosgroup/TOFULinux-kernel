/*
 *
 *  Realtek Bluetooth USB driver
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <linux/usb.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/poll.h>

#include <linux/version.h>
#include <linux/pm_runtime.h>
#include <linux/firmware.h>
#include <linux/suspend.h>


#define CONFIG_BLUEDROID        1 //bleuz 0 ;  bluedroid 1

#if CONFIG_BLUEDROID //for 4.2
#else //for blueZ	
	#include <net/bluetooth/bluetooth.h>
	#include <net/bluetooth/hci_core.h>
	#include <net/bluetooth/hci.h>
#endif


/***********************************
** Realtek - For rtk_btusb driver **
***********************************/
#define BTUSB_RPM		0* USB_RPM 	//	1 SS enable; 0 SS disable
#define LOAD_CONFIG		1         // set 1 if need to reconfig bt efuse
#define URB_CANCELING_DELAY_MS	10  	 // Added by Realtek
//when os suspend, module is still powered,usb is not powered, 
//this may set to 1 ,and must comply with special patch code
#define CONFIG_RESET_RESUME		1

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 33)
#define HDEV_BUS		hdev->bus
#define USB_RPM			1
#else
#define HDEV_BUS		hdev->type
#define USB_RPM			0
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 38)
#define NUM_REASSEMBLY 3
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 4, 0)
#define GET_DRV_DATA(x)		hci_get_drvdata(x)
#else
#define GET_DRV_DATA(x)		x->driver_data
#endif

static int patch_add(struct usb_interface* intf);
static void patch_remove(struct usb_interface* intf);
static int download_patch(struct usb_interface* intf);
static int set_btoff(struct usb_interface* intf);

#define BTUSB_MAX_ISOC_FRAMES	10
#define BTUSB_INTR_RUNNING		0
#define BTUSB_BULK_RUNNING		1
#define BTUSB_ISOC_RUNNING		2
#define BTUSB_SUSPENDING		3
#define BTUSB_DID_ISO_RESUME	4

struct btusb_data {
	struct hci_dev       *hdev;
	struct usb_device    *udev;
	struct usb_interface *intf;
	struct usb_interface *isoc;

	spinlock_t lock;

	unsigned long flags;

	struct work_struct work;
	struct work_struct waker;

	struct usb_anchor tx_anchor;
	struct usb_anchor intr_anchor;
	struct usb_anchor bulk_anchor;
	struct usb_anchor isoc_anchor;
	struct usb_anchor deferred;
	int tx_in_flight;
	spinlock_t txlock;

	struct usb_endpoint_descriptor *intr_ep;
	struct usb_endpoint_descriptor *bulk_tx_ep;
	struct usb_endpoint_descriptor *bulk_rx_ep;
	struct usb_endpoint_descriptor *isoc_tx_ep;
	struct usb_endpoint_descriptor *isoc_rx_ep;

	__u8 cmdreq_type;

	unsigned int sco_num;
	int isoc_altsetting;
	int suspend_count;

#if CONFIG_BLUEDROID //for 4.2
	wait_queue_head_t read_wait;
	struct sk_buff_head readq;
#endif
};

/* Realtek - For rtk_btusb driver end */

//========================================================================

#if CONFIG_BLUEDROID //for 4.2
#define SUCCESS               0 /* Linux success code */
#define ERROR                -1 /* Linux error code */
#define QUEUE_SIZE 100
static int btfcd_init(void);
static void btfcd_exit(void);
static int btfcd_open(struct inode *inode_p, struct file *file_p);
static int btfcd_close(struct inode *inode_p, struct file *file_p);
static ssize_t btfcd_read(struct file *file_p, char *buf_p, size_t count, loff_t *pos_p);
static ssize_t btfcd_write(struct file *file_p, const char *buf_p, size_t count, loff_t *pos_p);
static unsigned int btfcd_poll(struct file *file, poll_table *wait);

/*****************************************
** Realtek - Integrate from bluetooth.h **
*****************************************/
/* Reserv for core and drivers use */
#define BT_SKB_RESERVE	8

/* BD Address */
typedef struct {
	__u8 b[6];
} __packed bdaddr_t;

/* Skb helpers */
struct bt_skb_cb {
	__u8 pkt_type;
	__u8 incoming;
	__u16 expect;
	__u16 tx_seq;
	__u8 retries;
	__u8 sar;
	__u8 force_active;
};
#define bt_cb(skb) ((struct bt_skb_cb *)((skb)->cb))

static inline struct sk_buff *bt_skb_alloc(unsigned int len, gfp_t how)
{
	struct sk_buff *skb;

	if ((skb = alloc_skb(len + BT_SKB_RESERVE, how))) {
		skb_reserve(skb, BT_SKB_RESERVE);
		bt_cb(skb)->incoming  = 0;
	}
	return skb;
}
/* Realtek - Integrate from bluetooth.h end */

/***********************************
** Realtek - Integrate from hci.h **
***********************************/
#define HCI_MAX_ACL_SIZE	1024
#define HCI_MAX_SCO_SIZE	255
#define HCI_MAX_EVENT_SIZE	260
#define HCI_MAX_FRAME_SIZE	(HCI_MAX_ACL_SIZE + 4)

/* HCI bus types */
#define HCI_VIRTUAL	0
#define HCI_USB		1
#define HCI_PCCARD	2
#define HCI_UART	3
#define HCI_RS232	4
#define HCI_PCI		5
#define HCI_SDIO	6

/* HCI controller types */
#define HCI_BREDR	0x00
#define HCI_AMP		0x01

/* HCI device flags */
enum {
	HCI_UP,
	HCI_INIT,
	HCI_RUNNING,

	HCI_PSCAN,
	HCI_ISCAN,
	HCI_AUTH,
	HCI_ENCRYPT,
	HCI_INQUIRY,

	HCI_RAW,

	HCI_RESET,
};

/*
 * BR/EDR and/or LE controller flags: the flags defined here should represent
 * states from the controller.
 */
enum {
	HCI_SETUP,
	HCI_AUTO_OFF,
	HCI_MGMT,
	HCI_PAIRABLE,
	HCI_SERVICE_CACHE,
	HCI_LINK_KEYS,
	HCI_DEBUG_KEYS,
	HCI_UNREGISTER,

	HCI_LE_SCAN,
	HCI_SSP_ENABLED,
	HCI_HS_ENABLED,
	HCI_LE_ENABLED,
	HCI_CONNECTABLE,
	HCI_DISCOVERABLE,
	HCI_LINK_SECURITY,
	HCI_PENDING_CLASS,
};

/* HCI data types */
#define HCI_COMMAND_PKT		0x01
#define HCI_ACLDATA_PKT		0x02
#define HCI_SCODATA_PKT		0x03
#define HCI_EVENT_PKT		0x04
#define HCI_VENDOR_PKT		0xff

#define HCI_MAX_NAME_LENGTH		248
#define HCI_MAX_EIR_LENGTH		240

#define HCI_OP_READ_LOCAL_VERSION	0x1001
struct hci_rp_read_local_version {
	__u8     status;
	__u8     hci_ver;
	__le16   hci_rev;
	__u8     lmp_ver;
	__le16   manufacturer;
	__le16   lmp_subver;
} __packed;

#define HCI_EV_CMD_COMPLETE		0x0e
struct hci_ev_cmd_complete {
	__u8     ncmd;
	__le16   opcode;
} __packed;

/* ---- HCI Packet structures ---- */
#define HCI_COMMAND_HDR_SIZE 3
#define HCI_EVENT_HDR_SIZE   2
#define HCI_ACL_HDR_SIZE     4
#define HCI_SCO_HDR_SIZE     3

struct hci_command_hdr {
	__le16	opcode;		/* OCF & OGF */
	__u8	plen;
} __packed;

struct hci_event_hdr {
	__u8	evt;
	__u8	plen;
} __packed;

struct hci_acl_hdr {
	__le16	handle;		/* Handle & Flags(PB, BC) */
	__le16	dlen;
} __packed;

struct hci_sco_hdr {
	__le16	handle;
	__u8	dlen;
} __packed;

static inline struct hci_event_hdr *hci_event_hdr(const struct sk_buff *skb)
{
	return (struct hci_event_hdr *) skb->data;
}

static inline struct hci_acl_hdr *hci_acl_hdr(const struct sk_buff *skb)
{
	return (struct hci_acl_hdr *) skb->data;
}

static inline struct hci_sco_hdr *hci_sco_hdr(const struct sk_buff *skb)
{
	return (struct hci_sco_hdr *) skb->data;
}

/* ---- HCI Ioctl requests structures ---- */
struct hci_dev_stats {
	__u32 err_rx;
	__u32 err_tx;
	__u32 cmd_tx;
	__u32 evt_rx;
	__u32 acl_tx;
	__u32 acl_rx;
	__u32 sco_tx;
	__u32 sco_rx;
	__u32 byte_rx;
	__u32 byte_tx;
};
/* Realtek - Integrate from hci.h end */

/*****************************************
** Realtek - Integrate from hci_core.h  **
*****************************************/
struct hci_conn_hash {
	struct list_head list;
	unsigned int     acl_num;
	unsigned int     sco_num;
	unsigned int     le_num;
};

#define HCI_MAX_SHORT_NAME_LENGTH	10

#define NUM_REASSEMBLY 4
struct hci_dev {
	struct mutex	lock;

	char		name[8];
	unsigned long	flags;
	__u16		id;
	__u8		bus;
	__u8		dev_type;

	struct sk_buff		*reassembly[NUM_REASSEMBLY];

	struct hci_conn_hash	conn_hash;

	struct hci_dev_stats	stat;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
	atomic_t        refcnt;
	struct module           *owner;
	void                    *driver_data;
#endif

	atomic_t		promisc;

    struct device		*parent;
	struct device		dev;

	unsigned long		dev_flags;

	int (*open)(struct hci_dev *hdev);
	int (*close)(struct hci_dev *hdev);
	int (*flush)(struct hci_dev *hdev);
	int (*send)(struct sk_buff *skb);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
	void (*destruct)(struct hci_dev *hdev);
#endif
	void (*notify)(struct hci_dev *hdev, unsigned int evt);
	int (*ioctl)(struct hci_dev *hdev, unsigned int cmd, unsigned long arg);
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 4, 0)
static inline struct hci_dev *__hci_dev_hold(struct hci_dev *d)
{
        atomic_inc(&d->refcnt);
        return d;
}

static inline void __hci_dev_put(struct hci_dev *d)
{
        if (atomic_dec_and_test(&d->refcnt))
                d->destruct(d);
}
#endif

static inline void *hci_get_drvdata(struct hci_dev *hdev)
{
	return dev_get_drvdata(&hdev->dev);
}

static inline void hci_set_drvdata(struct hci_dev *hdev, void *data)
{
	dev_set_drvdata(&hdev->dev, data);
}

struct hci_dev *hci_dev_get(int index);

struct hci_dev *hci_alloc_dev(void);
void hci_free_dev(struct hci_dev *hdev);
int hci_register_dev(struct hci_dev *hdev);
void hci_unregister_dev(struct hci_dev *hdev);
int hci_dev_open(__u16 dev);
int hci_dev_close(__u16 dev);

int hci_recv_frame(struct sk_buff *skb);
int hci_recv_fragment(struct hci_dev *hdev, int type, void *data, int count);

#define SET_HCIDEV_DEV(hdev, pdev) ((hdev)->parent = (pdev))
/* Realtek - Integrate from hci_core.h end */

#endif //if bluedroid 4.2
