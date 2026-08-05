#include "server.h"
#include "../logging/log.h"
#include "../protocols/handshake.h"
#include <sodium/crypto_kx.h>
#include <sodium/utils.h>

int server_handshake(struct Node *node, volatile sig_atomic_t *running) {
    if (!node) {
        ERROR("Node cannot be null!");
        return 0;
    }

    crypto_kx_keypair(node->kx_public_key, node->kx_private_key);

    handshake_packet peer_packet;
    if (!handshake_recv(node->peer_fd, &peer_packet, running))
        goto fail;

    INFO("Received handshake from peer");

    if (!handshake_verify(&peer_packet))
        goto fail;

    INFO("Verified peer public key");

    handshake_packet packet;
    if (!handshake_prepare(&packet, node->kx_public_key, node))
        goto fail;

    if (!handshake_send(node->peer_fd, &packet))
        goto fail;

    if (crypto_kx_server_session_keys(node->session_rx, node->session_tx, node->kx_public_key, node->kx_private_key, peer_packet.kx_public_key) != 0) {
        ERROR("Failed to derive session keys");
        goto fail;
    }

    sodium_memzero(node->kx_private_key, sizeof(node->kx_private_key));
    return 1;

fail:
    sodium_memzero(node->kx_private_key, sizeof(node->kx_private_key));
    node_close_peer(node);
    return 0;
}
