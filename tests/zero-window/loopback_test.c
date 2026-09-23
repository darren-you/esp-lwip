// SPDX-License-Identifier: Apache-2.0
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/prot/tcp.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/tcp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Compile the actual dependency; no FRP, TLS, Wi-Fi or OS socket fixture.
 * Holding receive credit models two applications that have not read yet. */
static struct tcp_pcb *server, *client;
static unsigned packet_count, received[2];
static unsigned char payload[2][TCP_WND];
uint32_t lwip_test_initial_sequence = 10000;

int lwip_test_observe_packet(struct tcp_pcb *pcb, const struct tcp_hdr *header,
                           const struct pbuf *packet)
{
    if (pcb->state == LISTEN) return ERR_OK;
    if (++packet_count > 64) {
        fprintf(stderr, "lwIP loopback did not drain: packets=%u length=%u flags=%u "
                "seq_equals_rcv_nxt=%u receive_window=%u\n", packet_count,
                packet->tot_len, TCPH_FLAGS(header), header->seqno == pcb->rcv_nxt,
                (unsigned)pcb->rcv_wnd);
        /* A broken dependency must fail promptly instead of hanging CTest. */
        exit(EXIT_FAILURE);
    }
    return ERR_OK; /* Observation never consumes or changes a packet. */
}

static err_t receive(void *context, struct tcp_pcb *pcb, struct pbuf *packet, err_t error)
{
    (void)context;
    assert(error == ERR_OK && packet != NULL);
    unsigned side = pcb == server ? 0 : 1;
    assert(pcb == server || pcb == client);
    assert(received[side] + packet->tot_len <= TCP_WND);
    assert(pbuf_memcmp(packet, 0, payload[side] + received[side], packet->tot_len) == 0);
    received[side] += packet->tot_len;
    pbuf_free(packet);
    return ERR_OK;
}

static err_t accept_peer(void *context, struct tcp_pcb *pcb, err_t error)
{
    (void)context;
    assert(error == ERR_OK && server == NULL);
    server = pcb;
    tcp_recv(pcb, receive);
    return ERR_OK;
}

static err_t connected(void *context, struct tcp_pcb *pcb, err_t error)
{
    (void)context;
    assert(error == ERR_OK && pcb == client);
    tcp_recv(pcb, receive);
    return ERR_OK;
}

static void poll_packets(void)
{
    packet_count = 0;
    netif_poll_all();
}

static void check_empty_segment(int32_t offset, unsigned expected_packets)
{
    uint32_t sequence = client->snd_nxt;
    /* Only the test packet's sequence changes; restore the sending PCB before
     * input processing, so its real outstanding-byte state remains intact. */
    client->snd_nxt = sequence + (uint32_t)offset;
    assert(tcp_send_empty_ack(client) == ERR_OK);
    client->snd_nxt = sequence;
    poll_packets();
    assert(packet_count == expected_packets);
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "wrap")) lwip_test_initial_sequence = UINT32_MAX - 1440u;
    else assert(argc == 1);
    lwip_init();
    ip_addr_t address;
    IP_ADDR4(&address, 127, 0, 0, 1);
    struct tcp_pcb *listener = tcp_new();
    assert(listener && tcp_bind(listener, &address, 8765) == ERR_OK);
    listener = tcp_listen(listener);
    assert(listener);
    tcp_accept(listener, accept_peer);
    client = tcp_new();
    assert(client && tcp_connect(client, &address, 8765, connected) == ERR_OK);
    poll_packets();
    assert(server && server->state == ESTABLISHED && client->state == ESTABLISHED);

    for (unsigned round = 0; round < 100; ++round) {
        for (unsigned i = 0; i < TCP_WND; ++i) {
            payload[0][i] = (unsigned char)(round + i * 37u);
            payload[1][i] = (unsigned char)(round + i * 83u);
        }
        received[0] = received[1] = 0;
        assert(tcp_write(client, payload[0], TCP_WND, TCP_WRITE_FLAG_COPY) == ERR_OK);
        assert(tcp_output(client) == ERR_OK);
        assert(tcp_write(server, payload[1], TCP_WND, TCP_WRITE_FLAG_COPY) == ERR_OK);
        assert(tcp_output(server) == ERR_OK);
        poll_packets();
        assert(received[0] == TCP_WND && received[1] == TCP_WND);
        assert(server->rcv_wnd == 0 && client->rcv_wnd == 0);
        check_empty_segment(0, 1);  /* Zero window: RCV.NXT is acceptable. */
        check_empty_segment(-1, 2); /* Other sequence values require an ACK. */
        check_empty_segment(1, 2);
        tcp_recved(server, TCP_WND);
        tcp_recved(client, TCP_WND);
        poll_packets();
        assert(server->rcv_wnd == TCP_WND && client->rcv_wnd == TCP_WND);
    }
    check_empty_segment(0, 1);
    check_empty_segment(TCP_WND - 1, 1);
    check_empty_segment(-1, 2);
    check_empty_segment(TCP_WND, 2);
    tcp_abort(client);
    tcp_abort(server);
    assert(tcp_close(listener) == ERR_OK);
    poll_packets();
    puts("lwIP: 100 simultaneous zero-window/reopen rounds, exact bidirectional bytes passed");
    return EXIT_SUCCESS;
}
