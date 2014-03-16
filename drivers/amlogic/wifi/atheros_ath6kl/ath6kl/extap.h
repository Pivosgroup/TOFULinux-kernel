/*
 * Copyright (c) 2010, Atheros Communications Inc.
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

#ifndef __ATH6KL_EXTAP_H_
#define __ATH6KL_EXTAP_H_

typedef struct {
#if defined (__LITTLE_ENDIAN_BITFIELD)
    u8       ip_hl:4,
                    ip_version:4;
#elif defined (__BIG_ENDIAN_BITFIELD)
    u8       ip_version:4,
                    ip_hl:4;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
    u8       ip_tos;

    u16      ip_len;
    u16      ip_id;
    u16      ip_frag_off;
    u8       ip_ttl;
    u8       ip_proto;
    u16      ip_check;
    u32      ip_saddr;
    u32      ip_daddr;
    /*The options start here. */
 }net_iphdr_t;

#define ARP_REQ       1 /* ARP request */
#define ARP_RSP       2 /* ARP response */
#define ARP_RREQ          3 /* RARP request */
#define ARP_RRSP          4 /* RARP response */

#define NEXTHDR_ICMP     58 /* ICMP for IPv6. */

/* Neighbor Discovery */
#define ND_RSOL     133 /* Router Solicitation */
#define ND_RADVT        134 /* Router Advertisement */
#define ND_NSOL     135 /* Neighbor Solicitation */
#define ND_NADVT        136 /* Neighbor Advertisement */

/**
 * @brief IPv6 Address
 */
typedef struct {
    union {
        u8   u6_addr8[16];
        u16  u6_addr16[8];
        u32  u6_addr32[4];
    } in6_u;
#define s6_addr         in6_u.u6_addr8
#define s6_addr16       in6_u.u6_addr16
#define s6_addr32       in6_u.u6_addr32
} net_ipv6_addr_t;

/**
 * @brief IPv6 Header
 */
typedef struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
    u8       ipv6_priority:4,
                ipv6_version:4;
#elif defined(__BIG_ENDIAN_BITFIELD)
    u8       ipv6_version:4,
                ipv6_priority:4;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
    u8       ipv6_flow_lbl[3];

    u16      ipv6_payload_len;
    u8       ipv6_nexthdr,
                ipv6_hop_limit;

    net_ipv6_addr_t ipv6_saddr,
                ipv6_daddr;
} net_ipv6hdr_t;

/**
 * @brief ICMPv6 Header
 */
typedef struct {

    u8   icmp6_type;
    u8   icmp6_code;
    u16  icmp6_cksum;

    union {
        u32  un_data32[1];
        u16  un_data16[2];
        u8   un_data8[4];

        struct {
            u16  identifier;
            u16  sequence;
        } u_echo;

        struct {
#if defined(__LITTLE_ENDIAN_BITFIELD)
            u32  reserved:5,
                    override:1,
                    solicited:1,
                    router:1,
                    reserved2:24;
#elif defined(__BIG_ENDIAN_BITFIELD)
            u32  router:1,
                    solicited:1,
                    override:1,
                    reserved:29;
#else
#error  "Please fix <asm/byteorder.h>"
#endif  
        } u_nd_advt;

        struct {
            u8   hop_limit;
#if defined(__LITTLE_ENDIAN_BITFIELD)
            u8   reserved:6,
                    other:1,
                    managed:1;

#elif defined(__BIG_ENDIAN_BITFIELD)
            u8   managed:1,
                    other:1,
                    reserved:6;
#else
#error  "Please fix <asm/byteorder.h>"
#endif
            u16  rt_lifetime;
        } u_nd_ra;

    } icmp6_dataun;

} net_icmpv6hdr_t;

/**
 * @brief Neighbor Discovery Message
 */
typedef struct {
    net_icmpv6hdr_t nd_icmph;
    net_ipv6_addr_t nd_target;
    u8       nd_opt[0];
} net_nd_msg_t;

#define csum_ipv6(s, d, l, p, sum)          \
    csum_ipv6_magic((struct in6_addr *)s,       \
            (struct in6_addr *)d, l, p, sum)

#ifndef ETHERTYPE_PAE
#define ETHERTYPE_PAE   0x888e      /* EAPOL PAE/802.1x */
#endif
#ifndef ETHERTYPE_IP
#define ETHERTYPE_IP    0x0800      /* IP protocol */
#endif
#ifndef ETHERTYPE_AARP
#define ETHERTYPE_AARP  0x80f3      /* Appletalk AARP protocol */
#endif
#ifndef ETHERTYPE_IPX
#define ETHERTYPE_IPX   0x8137      /* IPX over DIX protocol */
#endif
#ifndef ETHERTYPE_ARP
#define ETHERTYPE_ARP   0x0806      /* ARP protocol */
#endif
#ifndef ETHERTYPE_IPV6
#define ETHERTYPE_IPV6  0x86dd      /* IPv6 */
#endif
#ifndef ETHERTYPE_VLAN
#define ETHERTYPE_VLAN  0x8100      /* VLAN TAG protocol */
#endif


#	define eamstr		"%02x:%02x:%02x:%02x:%02x:%02x"
#	define eamac(a)		(a)[0], (a)[1], (a)[2],\
				(a)[3], (a)[4], (a)[5]
#	define eaistr		"%u.%u.%u.%u"
#	define eaip(a)		((a)[0] & 0xff), ((a)[1] & 0xff),\
				((a)[2] & 0xff), ((a)[3] & 0xff)
#	define eastr6		"%02x%02x:%02x%02x:%02x%02x:"\
				"%02x%02x:%02x%02x:%02x%02x:"\
				"%02x%02x:%02x%02x"
#	define eaip6(a)		(a)[0], (a)[1], (a)[2], (a)[3],\
				(a)[4], (a)[5], (a)[6], (a)[7],\
				(a)[8], (a)[9], (a)[10], (a)[11],\
				(a)[12], (a)[13], (a)[14], (a)[15]

#ifdef EXTAP_DEBUG
#	define eadbg1(...)	IEEE80211_DPRINTF(NULL, 0, __VA_ARGS__)
#	define eadbg2(b, c)				\
	do {						\
		IEEE80211_DPRINTF(NULL, 0,		\
			"%s(%d): replacing " #b " "	\
			eamstr " with " eamstr "\n",	\
			__func__, __LINE__,		\
			eamac(b), eamac(c));		\
	} while (0)

#	define eadbg2i(b, c, i)				\
	do {						\
		IEEE80211_DPRINTF(NULL, 0,		\
			"%s(%d): replacing " #b " "	\
			eamstr " with " eamstr		\
			" for " eaistr "\n",		\
			__func__, __LINE__,		\
			eamac(b), eamac(c), eaip(i));	\
	} while (0)

#	define eadbg3(...)	IEEE80211_DPRINTF(NULL, 0, __VA_ARGS__)

#	define print_arp(a)		\
		print_arp_pkt(__func__, __LINE__, a)
#	define mi_add(a, b, c, d)	\
		mi_tbl_add(__func__, __LINE__, a, b, c, d)
#	define mi_lkup(a, b, c)		\
		mi_tbl_lkup(__func__, __LINE__, a, b, c)
#	define print_ipv6	print_ipv6_pkt


#else
#	define eadbg1(...)	/* */
#	define eadbg2(...)	/* */
#	define eadbg2i(...)	/* */
#	define eadbg3(...)	/* */
#	define print_arp(...)	/* */
#	define print_ipv6(...)	/* */
//#	define print_ipv6	print_ipv6_pkt
#	define mi_add		mi_tbl_add
#	define mi_lkup		mi_tbl_lkup
#endif

typedef struct {
	unsigned short	ar_hrd,	/* format of hardware address */
			ar_pro;	/* format of protocol address */
	unsigned char	ar_hln,	/* length of hardware address */
			ar_pln;	/* length of protocol address */
	unsigned short	ar_op;	/* ARP opcode (command) */
	unsigned char	ar_sha[ETH_ALEN],	/* sender hardware address */
			ar_sip[4],		/* sender IP address */
			ar_tha[ETH_ALEN],	/* target hardware address */
			ar_tip[4];		/* target IP address */
} eth_arphdr_t;

typedef struct {
	unsigned char	type,
			len,
			addr[ETH_ALEN];	/* hardware address */
} eth_icmp6_lladdr_t;

int ath6kl_extap_input(struct ath6kl_vif *, struct ethhdr *);
int ath6kl_extap_output(struct ath6kl_vif *, struct ethhdr *);



#endif /* __ATH6KL_EXTAP_H_ */
