/*	$Id$	*/

/*
 * Copyright (c) 2004,2005 Damien Miller <djm@mindrot.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* NetFlow packet definitions */

#ifndef _NETFLOW_H
#define _NETFLOW_H

/*
 * These are Cisco Netflow(tm) packet formats
 * Based on:
 * http://www.cisco.com/univercd/cc/td/doc/product/rtrmgmt/nfc/nfc_3_0/nfc_ug/nfcform.htm
 */

/* Common header fields */
#pragma pack(push, 1)
struct NF_HEADER_COMMON {
    uint16_t version, flows;
};
#pragma pack(pop)

/* Netflow v.1 */
#pragma pack(push, 1)
struct NF1_HEADER {
    struct NF_HEADER_COMMON c;
    uint32_t uptime_ms, time_sec, time_nanosec;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct NF1_FLOW {
    uint32_t src_ip, dest_ip, nexthop_ip;
    uint16_t if_index_in, if_index_out;
    uint32_t flow_packets, flow_octets;
    uint32_t flow_start, flow_finish;
    uint16_t src_port, dest_port;
    uint16_t pad1;
    uint8_t protocol, tos, tcp_flags;
    uint8_t pad2, pad3, pad4;
    uint32_t reserved1;
#if 0
 	uint8_t reserved2; /* XXX: no longer used */
#endif
};
#pragma pack(pop)

/* Maximum of 30 flows per packet */
#define NF1_MAXFLOWS 24
#define NF1_PACKET_SIZE(nflows) (sizeof(struct NF1_HEADER) + ((nflows) * sizeof(struct NF1_FLOW)))
#define NF1_MAXPACKET_SIZE (NF1_PACKET_SIZE(NF1_MAXFLOWS))

/* Netflow v.5 */
#pragma pack(push, 1)
struct NF5_HEADER {
    struct NF_HEADER_COMMON c;
    uint32_t uptime_ms, time_sec, time_nanosec, flow_sequence;
    uint8_t engine_type, engine_id, reserved1, reserved2;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct NF5_FLOW {
    uint32_t src_ip, dest_ip, nexthop_ip;
    uint16_t if_index_in, if_index_out;
    uint32_t flow_packets, flow_octets;
    uint32_t flow_start, flow_finish;
    uint16_t src_port, dest_port;
    uint8_t pad1;
    uint8_t tcp_flags, protocol, tos;
    uint16_t src_as, dest_as;
    uint8_t src_mask, dst_mask;
    uint16_t pad2;
};
#pragma pack(pop)
/* Maximum of 24 flows per packet */
#define NF5_MAXFLOWS 30
#define NF5_PACKET_SIZE(nflows) (sizeof(struct NF5_HEADER) + ((nflows) * sizeof(struct NF5_FLOW)))
#define NF5_MAXPACKET_SIZE (NF5_PACKET_SIZE(NF5_MAXFLOWS))

/* Netflow v.7 */
#pragma pack(push, 1)
struct NF7_HEADER {
    struct NF_HEADER_COMMON c;
    uint32_t uptime_ms, time_sec, time_nanosec, flow_sequence;
    uint32_t reserved1;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct NF7_FLOW {
    uint32_t src_ip, dest_ip, nexthop_ip;
    uint16_t if_index_in, if_index_out;
    uint32_t flow_packets, flow_octets;
    uint32_t flow_start, flow_finish;
    uint16_t src_port, dest_port;
    uint8_t flags1;
    uint8_t tcp_flags, protocol, tos;
    uint16_t src_as, dest_as;
    uint8_t src_mask, dst_mask;
    uint16_t flags2;
    uint32_t router_sc;
};
#pragma pack(pop)

/* Maximum of 24 flows per packet */
#define NF7_MAXFLOWS 30
#define NF7_PACKET_SIZE(nflows) (sizeof(struct NF7_HEADER) + ((nflows) * sizeof(struct NF7_FLOW)))
#define NF7_MAXPACKET_SIZE (NF7_PACKET_SIZE(NF7_MAXFLOWS))

/* Netflow v.9 */
#pragma pack(push, 1)
struct NF9_HEADER {
    struct NF_HEADER_COMMON c;
    uint32_t uptime_ms, time_sec;
    uint32_t package_sequence, source_id;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct NF9_FLOWSET_HEADER_COMMON {
    uint16_t flowset_id, length;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct NF9_TEMPLATE_FLOWSET_HEADER {
    uint16_t template_id, count;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct NF9_TEMPLATE_FLOWSET_RECORD {
    uint16_t type, length;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct NF9_DATA_FLOWSET_HEADER {
    struct NF9_FLOWSET_HEADER_COMMON c;
};
#pragma pack(pop)

#define NF9_TEMPLATE_FLOWSET_ID 0
#define NF9_OPTIONS_FLOWSET_ID 1
#define NF9_MIN_RECORD_FLOWSET_ID 256

/* Flowset record types the we care about */
#define NF9_IN_BYTES 1
#define NF9_IN_PACKETS 2
/* ... */
#define NF9_IN_PROTOCOL 4
#define NF9_SRC_TOS 5
#define NF9_TCP_FLAGS 6
#define NF9_L4_SRC_PORT 7
#define NF9_IPV4_SRC_ADDR 8
#define NF9_SRC_MASK 9
#define NF9_INPUT_SNMP 10
#define NF9_L4_DST_PORT 11
#define NF9_IPV4_DST_ADDR 12
#define NF9_DST_MASK 13
#define NF9_OUTPUT_SNMP 14
#define NF9_IPV4_NEXT_HOP 15
#define NF9_SRC_AS 16
#define NF9_DST_AS 17
/* ... */
#define NF9_LAST_SWITCHED 21
#define NF9_FIRST_SWITCHED 22
/* ... */
#define NF9_IPV6_SRC_ADDR 27
#define NF9_IPV6_DST_ADDR 28
#define NF9_IPV6_SRC_MASK 29
#define NF9_IPV6_DST_MASK 30
/* ... */
#define NF9_ENGINE_TYPE 38
#define NF9_ENGINE_ID 39
/* ... */
#define NF9_IP_PROTOCOL_VERSION 60
#define NF9_DIRECTION 61
#define NF9_IPV6_NEXT_HOP 62

/* Netflow v.10 */
#pragma pack(push, 1)
struct NF10_HEADER {
    struct NF_HEADER_COMMON c;
    uint32_t time_sec;
    uint32_t package_sequence, source_id;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct NF10_FLOWSET_HEADER_COMMON {
    uint16_t flowset_id, length;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct NF10_TEMPLATE_FLOWSET_HEADER {
    uint16_t template_id, count;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct NF10_TEMPLATE_FLOWSET_RECORD {
    uint16_t type, length;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct NF10_DATA_FLOWSET_HEADER {
    struct NF10_FLOWSET_HEADER_COMMON c;
};
#pragma pack(pop)

#define NF10_TEMPLATE_FLOWSET_ID 2
#define NF10_OPTIONS_FLOWSET_ID 3
#define NF10_MIN_RECORD_FLOWSET_ID 256

#define NF10_ENTERPRISE (1 << 15)

/* Flowset record types the we care about */
#define NF10_IN_BYTES 1
#define NF10_IN_PACKETS 2
/* ... */
#define NF10_IN_PROTOCOL 4
#define NF10_SRC_TOS 5
#define NF10_TCP_FLAGS 6
#define NF10_L4_SRC_PORT 7
#define NF10_IPV4_SRC_ADDR 8
#define NF10_SRC_MASK 9
#define NF10_INPUT_SNMP 10
#define NF10_L4_DST_PORT 11
#define NF10_IPV4_DST_ADDR 12
#define NF10_DST_MASK 13
#define NF10_OUTPUT_SNMP 14
#define NF10_IPV4_NEXT_HOP 15
#define NF10_SRC_AS 16
#define NF10_DST_AS 17
/* ... */
#define NF10_LAST_SWITCHED 21
#define NF10_FIRST_SWITCHED 22
/* ... */
#define NF10_IPV6_SRC_ADDR 27
#define NF10_IPV6_DST_ADDR 28
#define NF10_IPV6_SRC_MASK 29
#define NF10_IPV6_DST_MASK 30
/* ... */
#define NF10_ENGINE_TYPE 38
#define NF10_ENGINE_ID 39
/* ... */
#define NF10_IP_PROTOCOL_VERSION 60
#define NF10_DIRECTION 61
#define NF10_IPV6_NEXT_HOP 62

/* A record in a NetFlow v.9 template record */
struct peer_nf9_record {
    uint32_t type{0};
    uint32_t len{0};

    peer_nf9_record(uint32_t type, uint32_t len)
        : type(type)
        , len(len)
    {
    }
};

/* A NetFlow v.9 template record */
struct peer_nf9_template {
    uint16_t template_id;
    uint32_t source_id;
    uint32_t num_records;
    uint32_t total_len;
    std::vector<peer_nf9_record> records;
};

/* A record in a NetFlow v.10 template record */
struct peer_nf10_record {
    uint32_t type{0};
    uint32_t len{0};

    peer_nf10_record(uint32_t type, uint32_t len)
        : type(type)
        , len(len)
    {
    }
};

/* A NetFlow v.10 template record */
struct peer_nf10_template {
    uint16_t template_id;
    uint32_t source_id;
    uint32_t num_records;
    uint32_t total_len;
    std::vector<peer_nf10_record> records;
};

#endif /* _NETFLOW_H */
