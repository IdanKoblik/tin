#pragma once

#include <sodium.h>

int encrypt_buffer(uint8_t *ciphertext, unsigned long long *ciphertext_len,
                   const uint8_t *message, size_t message_len,
                   const uint8_t nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES],
                   const unsigned char session_tx[crypto_kx_SESSIONKEYBYTES]
);

int decrypt_buffer(uint8_t *decrypted, unsigned long long *decrypted_len,
                   const uint8_t *ciphertext, size_t ciphertext_len,
                   const uint8_t nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES],
                   const unsigned char session_rx[crypto_kx_SESSIONKEYBYTES]
);
