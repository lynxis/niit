/*
 * niitv2.c
 *
 *  Created on: 11.10.2009
 *      Author: lynxis\
 *  TODO: device Hoplimit is max device ttl ?!
 *  TODO: mtu
 *  TODO: ipv4 route check
 */

#include <linux/skbuff.h>
#include <linux/if.h>
#include <linux/module.h>
#include <linux/capability.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sockios.h>

#include <linux/net.h>
#include <linux/in6.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/icmp.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/netfilter_ipv4.h>
#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <net/sock.h>
#include <net/snmp.h>

#include <net/ipv6.h>
#include <net/protocol.h>
#include <net/transp_v6.h>
#include <net/ip6_fib.h>
#include <net/ip6_route.h>
#include <net/ip6_tunnel.h>
#include <net/ndisc.h>
#include <net/addrconf.h>
#include <net/ip.h>
#include <net/ip6_fib.h>
#include <net/udp.h>
#include <net/icmp.h>
#include <net/ip6_route.h>
#include <net/ipip.h>
#include <net/inet_ecn.h>
#include <net/xfrm.h>
#include <net/dsfield.h>
#include <net/net_namespace.h>
#include <net/netns/generic.h>
#include <net/dsfield.h>
#include <net/dst.h>
#include <linux/if.h>
#include <linux/version.h>

#include "niit.h"

#define static

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)

static inline struct dst_entry *skb_dst(const struct sk_buff *skb) {
    return (struct dst_entry *) skb->dst;
}

#endif

struct net_device* tunnel_dev;

struct niit_tunnel {
    __be32 ipv6prefix_1;
    __be32 ipv6prefix_2;
    __be32 ipv6prefix_3;
    int recursion;
};

static int niit_err(struct sk_buff *skb, u32 info) {
    return 0;
}

static int niit_xmit(struct sk_buff *skb, struct net_device *dev) {

    struct niit_tunnel *tunnel = (struct niit_tunnel *) netdev_priv(tunnel_dev);
    struct net_device_stats *stats = &tunnel_dev->stats;
    struct iphdr *iph4;
    struct ipv6hdr *iph6;

    struct rt6_info *rt6; /* Route to the other host */
    struct net_device *tdev; /* Device to other host */
    __u8 nexthdr; /* IPv6 next header */
    unsigned int len; /* skb len */
    u32 delta; /* calc space inside skb */
    unsigned int max_headroom; /* The extra header space needed */
    const unsigned char *old_mac;
    unsigned int mtu = tunnel_dev->mtu;
    int skb_delta;
    struct in6_addr s6addr;
    struct in6_addr d6addr;

    if (tunnel->recursion++) {
        stats->collisions++;
        PDEBUG("niit recursion detected beforce skb->protocol switch \n");
        goto tx_error;
    }

    /*
     * all IPv4 (includes icmp) will be encapsulated.
     * IPv6 ICMPs for IPv4 encapsulated data should be translated
     *
     */
    if (skb->protocol == htons(ETH_P_IP)) {

        PDEBUG("niit skb->proto = iph4 \n");
        iph4 = ip_hdr(skb);

        s6addr.in6_u.u6_addr32[0] = tunnel->ipv6prefix_1;
        s6addr.in6_u.u6_addr32[1] = tunnel->ipv6prefix_2;
        s6addr.in6_u.u6_addr32[2] = tunnel->ipv6prefix_3;
        s6addr.in6_u.u6_addr32[3] = iph4->saddr;

        d6addr.in6_u.u6_addr32[0] = tunnel->ipv6prefix_1;
        d6addr.in6_u.u6_addr32[1] = tunnel->ipv6prefix_2;
        d6addr.in6_u.u6_addr32[2] = tunnel->ipv6prefix_3;
        d6addr.in6_u.u6_addr32[3] = iph4->daddr;

        PDEBUG("niit: ipv4: saddr: %x%x%x%x \n niit: ipv4: daddr %x%x%x%x \n",
         s6addr.in6_u.u6_addr32[0], s6addr.in6_u.u6_addr32[1],
         s6addr.in6_u.u6_addr32[2], s6addr.in6_u.u6_addr32[3],
         d6addr.in6_u.u6_addr32[0], d6addr.in6_u.u6_addr32[1],
         d6addr.in6_u.u6_addr32[2], d6addr.in6_u.u6_addr32[3]);

        if ((rt6 = rt6_lookup(dev_net(tunnel_dev), &d6addr, &s6addr, (tunnel_dev)->iflink, 0)) == NULL) {
            stats->tx_carrier_errors++;
            goto tx_error_icmp;
        }

        tdev = rt6->u.dst.dev;

        if (tdev == dev) {
            PDEBUG("niit recursion detected todev = dev \n");
            stats->collisions++;
            goto tx_error;
        }
        /*
         mtu = skb_dst(skb) ? dst_mtu(skb_dst(skb)) : dev->mtu;

         if (mtu < IPV6_MIN_MTU)
         mtu = IPV6_MIN_MTU;
         *//*
         if (skb->len > mtu) {
         PDEBUG("niit: dropped paket\n");
         goto tx_error;
         } */
        /*
         if (tunnel->err_count > 0) {
         if (time_before(jiffies,
         tunnel->err_time + IPTUNNEL_ERR_TIMEO)) {
         tunnel->err_count--;
         dst_link_failure(skb);
         } else
         tunnel->err_count = 0;
         }
         */
        /*
         * Okay, now see if we can stuff it in the buffer as-is.
         */
        max_headroom = LL_RESERVED_SPACE(tdev) + sizeof(struct ipv6hdr);

        if (skb_headroom(skb) < max_headroom || skb_shared(skb) || (skb_cloned(skb) && !skb_clone_writable(skb, 0))) {
            struct sk_buff *new_skb = skb_realloc_headroom(skb, max_headroom);
            if (!new_skb) {
                stats->tx_dropped++;
                dev_kfree_skb(skb);
                tunnel->recursion--;
                return 0;
            }
            if (skb->sk)
                skb_set_owner_w(new_skb, skb->sk);
            dev_kfree_skb(skb);
            skb = new_skb;
            iph4 = ip_hdr(skb);
        }

        delta = skb_network_header(skb) - skb->data;

        /* we need some space */
        if (delta < sizeof(struct ipv6hdr)) {
            iph6 = (struct ipv6hdr*) skb_push(skb, sizeof(struct ipv6hdr) - delta);
            PDEBUG("niit : iph6 < 0 skb->len %x \n", skb->len);
        }
        else if (delta > sizeof(struct ipv6hdr)) {
            iph6 = (struct ipv6hdr*) skb_pull(skb, delta - sizeof(struct ipv6hdr));
            PDEBUG("niit : iph6 > 0 skb->len %x \n", skb->len);
        }
        else {
            iph6 = (struct ipv6hdr*) skb->data;
            PDEBUG("niit : iph6 = 0 skb->len %x \n", skb->len);
        }

        /* skb->network_header iph6; */
        /* skb->transport_header iph4; */
        skb->transport_header = skb->network_header; /* we say skb->transport_header = iph4; */
        skb_reset_network_header(skb); /* now -> we reset the network header to skb->data which is our ipv6 paket */
        skb_reset_mac_header(skb);

        /* prepare to send it again */
        IPCB(skb)->flags = 0;
        skb->protocol = htons(ETH_P_IPV6);
        skb->pkt_type = PACKET_HOST;
        skb->dev = tunnel_dev;
        dst_release(skb->dst);
        skb->dst = NULL;

        /* install v6 header */
        memset(iph6, 0, sizeof(struct ipv6hdr));
        iph6->version = 6;
        iph6->payload_len = iph4->tot_len;
        iph6->hop_limit = iph4->ttl;
        iph6->nexthdr = IPPROTO_IPIP;
        memcpy(&(iph6->saddr), &s6addr, sizeof(struct in6_addr));
        memcpy(&(iph6->daddr), &d6addr, sizeof(struct in6_addr));

        PDEBUG("niit : before nf_reset skb->len %x \n", skb->len);PDEBUG("niit : before nf_reset mac->len  %x \n", skb->mac_len);PDEBUG("niit : before nf_reset data->len  %x \n", skb->data_len);

        nf_reset(skb);
        netif_rx(skb);
        tunnel->recursion--;
    }
    else if (skb->protocol == htons(ETH_P_IPV6)) {
        __be32 s4addr;
        __be32 d4addr;
        __u8 hoplimit;
        struct rtable *rt; /* Route to the other host */
        PDEBUG("niit skb->proto = iph6 \n");

        iph6 = ipv6_hdr(skb);
        if (!iph6) {
            PDEBUG("niit cant find iph6 \n");
            goto tx_error;
        }

        /* IPv6 to IPv4 */
        hoplimit = iph6->hop_limit;

        if (iph6->daddr.s6_addr32[0] != tunnel->ipv6prefix_1 || iph6->daddr.s6_addr32[1] != tunnel->ipv6prefix_2
                || iph6->daddr.s6_addr32[2] != tunnel->ipv6prefix_3) {
            PDEBUG("niit_xmit ipv6(): Dst addr haven't our previx addr: %x%x%x%x, packet dropped.\n",
                    iph6->daddr.s6_addr32[0], iph6->daddr.s6_addr32[1],
                    iph6->daddr.s6_addr32[2], iph6->daddr.s6_addr32[3]);
            goto tx_error;
        }

        s4addr = iph6->saddr.s6_addr32[3];
        d4addr = iph6->daddr.s6_addr32[3];
        nexthdr = iph6->nexthdr;
        /* TODO nexthdr */
        /*
         while(nexthdr != IPPROTO_IPIP) {

         }
         */

        iph4 = ipip_hdr(skb);

        /* check for a valid route */
        /*	{
         struct flowi fl = { .nl_u = { .ip4_u =
         { .daddr = d4addr,
         .saddr = s4addr,
         .tos = RT_TOS(iph4->tos) } },
         .oif = tunnel_dev->iflink,
         .proto = iph4->protocol };

         if (ip_route_output_key(dev_net(dev), &rt, &fl)) {
         PDEBUG("niit : ip route not found \n");
         stats->tx_carrier_errors++;
         goto tx_error_icmp;
         }
         }
         tdev = rt->u.dst.dev;
         if (tdev == tunnel_dev) {
         PDEBUG("niit : tdev == tunnel_dev \n");
         ip_rt_put(rt);
         stats->collisions++;
         goto tx_error;
         }

         if (iph4->frag_off)
         mtu = dst_mtu(&rt->u.dst) - sizeof(struct iphdr);
         else
         mtu = skb_dst(skb) ? dst_mtu(skb_dst(skb)) : dev->mtu;

         if (mtu < 68) {
         PDEBUG("niit : mtu < 68 \n");
         stats->collisions++;
         ip_rt_put(rt);
         goto tx_error;
         }
         if (iph4->daddr && skb_dst(skb))
         skb_dst(skb)->ops->update_pmtu(skb_dst(skb), mtu);
         */
        /*
         if (skb->len > mtu) {
         icmpv6_send(skb, ICMPV6_PKT_TOOBIG, 0, mtu, dev);
         ip_rt_put(rt);
         goto tx_error;
         }
         */

        /*
         * Okay, now see if we can stuff it in the buffer as-is.
         */

        //        max_headroom = LL_RESERVED_SPACE(tdev)+sizeof(struct iphdr);
        PDEBUG("niit: eret 1 \n");

        if (skb_shared(skb) || (skb_cloned(skb) && !skb_clone_writable(skb, 0))) {
            struct sk_buff *new_skb = skb_realloc_headroom(skb, skb_headroom(skb));
            if (!new_skb) {
                stats->tx_dropped++;
                dev_kfree_skb(skb);
                tunnel->recursion--;
                return 0;
            }
            if (skb->sk)
                skb_set_owner_w(new_skb, skb->sk);
            dev_kfree_skb(skb);
            skb = new_skb;
            iph6 = ipv6_hdr(skb);
            iph4 = ipip_hdr(skb);
        }

        delta = skb_transport_header(skb) - skb->data;
        skb_pull(skb, delta);

        /* our paket come with ... */
        /* skb->network_header iph6; */
        /* skb->transport_header iph4; */
        skb->network_header = skb->transport_header; /* we say skb->network_header = iph4; */
        skb_set_transport_header(skb, sizeof(struct iphdr));
        skb_reset_mac_header(skb);

        /* prepare to send it again */
        IPCB(skb)->flags = 0;
        skb->protocol = htons(ETH_P_IP);
        skb->pkt_type = PACKET_HOST;
        skb->dev = tunnel_dev;
        dst_release(skb->dst);
        skb->dst = NULL;

        /* TODO: set iph4->ttl = hoplimit and recalc the checksum ! */

        /* sending */
        nf_reset(skb);
        netif_rx(skb);
        tunnel->recursion--;
    }
    else {
        PDEBUG("niit unknown direction %x \n", skb->protocol);
        goto tx_error;
        /* drop */
    }PDEBUG("niit: ret 0 \n");
    return 0;

    tx_error_icmp: dst_link_failure(skb);
    PDEBUG("niit: tx_error_icmp\n");
    tx_error: PDEBUG("niit: tx_error\n");
    stats->tx_errors++;
    dev_kfree_skb(skb);
    tunnel->recursion--;
    return 0;
}

static int niit_tunnel_change_mtu(struct net_device *dev, int new_mtu) {
    if (new_mtu < IPV6_MIN_MTU || new_mtu > 0xFFF8 - sizeof(struct iphdr))
        return -EINVAL;
    dev->mtu = new_mtu;
    return 0;
}

#ifdef HAVE_NET_DEVICE_OPS
static const struct net_device_ops niit_netdev_ops = {
        .ndo_start_xmit = niit_xmit,
};
#else
static void niit_regxmit(struct net_device *dev) {
    dev->hard_start_xmit = niit_xmit;
}
#endif

static void niit_dev_setup(struct net_device *dev) {

    ether_setup(dev);

    memset(netdev_priv(dev), 0, sizeof(struct niit_tunnel));

#ifdef HAVE_NET_DEVICE_OPS
    dev->netdev_ops = &niit_netdev_ops;
#endif
    dev->destructor = free_netdev;
    dev->type = ARPHRD_TUNNEL;
    dev->mtu = ETH_DATA_LEN - sizeof(struct ipv6hdr);
    dev->flags = IFF_NOARP;
    dev->iflink = 1;

}

static void __exit niit_cleanup(void) {

    rtnl_lock();
    unregister_netdevice(tunnel_dev);
    rtnl_unlock();

}

static int __init niit_init(void) {
    int err;
    struct niit_tunnel *tunnel;
    printk(KERN_INFO "network IPv6 over IPv4 tunneling driver\n");

    err = -ENOMEM;
    tunnel_dev = alloc_netdev(sizeof(struct niit_tunnel), "niit0",
            niit_dev_setup);
    if (!tunnel_dev) {
        err = -ENOMEM;
        goto err_alloc_dev;
    }
    tunnel = (struct niit_tunnel *) netdev_priv(tunnel_dev);

    if ((err = register_netdev(tunnel_dev)))
        goto err_reg_dev;

#ifndef HAVE_NET_DEVICE_OPS
    niit_regxmit(tunnel_dev);
#endif

    tunnel->ipv6prefix_1 = htonl(NIIT_V6PREFIX_1);
    tunnel->ipv6prefix_2 = htonl(NIIT_V6PREFIX_2);
    tunnel->ipv6prefix_3 = htonl(NIIT_V6PREFIX_3);

    return 0;

    err_reg_dev:
    dev_put(tunnel_dev);
    free_netdev(tunnel_dev);
    err_alloc_dev:
    return err;

}

module_init( niit_init);
module_exit( niit_cleanup);
MODULE_LICENSE("GPL");
MODULE_ALIAS("niit0");