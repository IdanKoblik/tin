#include "crypto/buffer.h"

int encrypt_buffer(uint8_t *ciphertext, unsigned long long *ciphertext_len,
                   const uint8_t *message, size_t message_len,
                   const uint8_t nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES],
                   const unsigned char session_tx[crypto_kx_SESSIONKEYBYTES])
{
    return crypto_aead_chacha20poly1305_ietf_encrypt(
        ciphertext, ciphertext_len,
        message, message_len,
        NULL, 0, // No public associated data
        NULL, nonce, session_tx
    );
}

int decrypt_buffer(uint8_t *decrypted, unsigned long long *decrypted_len,
                   const uint8_t *ciphertext, size_t ciphertext_len,
                   const uint8_t nonce[crypto_aead_chacha20poly1305_ietf_NPUBBYTES],
                   const unsigned char session_rx[crypto_kx_SESSIONKEYBYTES])
{
    return crypto_aead_chacha20poly1305_ietf_decrypt(
        decrypted, decrypted_len,
        NULL,
        ciphertext, ciphertext_len,
        NULL, 0,
        nonce, session_rx
    );
}
