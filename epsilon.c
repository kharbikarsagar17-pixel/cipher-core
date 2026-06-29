/*
 * Epsilon Module - Network Protocol Handler
 * Part of Cipher-Core Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EPSILON_VERSION "1.0.0"
#define MAX_PACKETS 64

typedef struct {
    int id;
    int src_port;
    int dst_port;
    char payload[256];
    int payload_len;
} EpsilonPacket;

static EpsilonPacket packet_pool[MAX_PACKETS];
static int packet_count = 0;

static void epsilon_init(void) {
    memset(packet_pool, 0, sizeof(packet_pool));
    printf("Epsilon network protocol module initialized v%s\n", EPSILON_VERSION);
}

static int epsilon_send_packet(int src, int dst, const char *data) {
    if (packet_count >= MAX_PACKETS) return -1;
    
    EpsilonPacket *p = &packet_pool[packet_count++];
    p->id = packet_count;
    p->src_port = src;
    p->dst_port = dst;
    strncpy(p->payload, data, 255);
    p->payload_len = strlen(data);
    
    return p->id;
}

static void epsilon_list_packets(void) {
    printf("Epsilon Packet Queue:\n");
    for (int i = 0; i < packet_count; i++) {
        printf("  [%d] %d -> %d: %s\n", 
               packet_pool[i].id,
               packet_pool[i].src_port,
               packet_pool[i].dst_port,
               packet_pool[i].payload);
    }
}
