// SPDX-License-Identifier: Apache-2.0
#include <stdint.h>
struct tcp_pcb;
struct tcp_hdr;
struct pbuf;
int lwip_test_observe_packet(struct tcp_pcb *, const struct tcp_hdr *, const struct pbuf *);
#define LWIP_HOOK_TCP_INPACKET_PCB(pcb, hdr, optlen, opt1len, opt2, p) \
    lwip_test_observe_packet(pcb, hdr, p)
extern uint32_t lwip_test_initial_sequence;
#define LWIP_HOOK_TCP_ISN(local_ip, local_port, remote_ip, remote_port) lwip_test_initial_sequence
