#include "crypto/ed25519.h"
#include "net/node.h"
#include "logging/log.h"
#include <errno.h>
#include <fcntl.h>
#include <sodium/crypto_sign.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PUB_HEADER "-----BEGIN SODIUM SIGN PUBLIC KEY-----"
#define PUB_FOOTER "-----END SODIUM SIGN PUBLIC KEY-----"

#define SEC_HEADER "-----BEGIN SODIUM SIGN SECRET KEY-----"
#define SEC_FOOTER "-----END SODIUM SIGN SECRET KEY-----"

#define PUB_FILE "public.pem"
#define SEC_FILE "private.pem"

#define B64_VARIANT sodium_base64_VARIANT_ORIGINAL

#define PEM_LINE_MAX (sodium_base64_ENCODED_LEN(crypto_sign_SECRETKEYBYTES, B64_VARIANT) + 2)

static int pem_line(FILE *f, char *line, size_t size) {
    if (!fgets(line, (int)size, f))
        return 0;

    line[strcspn(line, "\r\n")] = 0;
    return 1;
}

/* Returns 1 on success, 0 if the file does not exist, -1 if it exists but is unusable. */
static int pem_read(const char *file, const char *header, const char *footer, unsigned char *out, size_t out_len) {
    FILE *f = fopen(file, "r");
    if (!f) {
        if (errno == ENOENT)
            return 0;

        ERROR("Failed to open %s", file);
        return -1;
    }

    char line[PEM_LINE_MAX];
    int rc = -1;

    if (!pem_line(f, line, sizeof(line)) || strcmp(line, header) != 0) {
        ERROR("%s: missing or bad header", file);
        goto done;
    }

    if (!pem_line(f, line, sizeof(line))) {
        ERROR("%s: missing key body", file);
        goto done;
    }

    size_t decoded = 0;
    if (sodium_base642bin(out, out_len, line, strlen(line), NULL, &decoded, NULL, B64_VARIANT) != 0) {
        ERROR("%s: key body is not valid base64", file);
        goto done;
    }

    if (decoded != out_len) {
        ERROR("%s: expected %zu key bytes, got %zu", file, out_len, decoded);
        goto done;
    }

    if (!pem_line(f, line, sizeof(line)) || strcmp(line, footer) != 0) {
        ERROR("%s: missing or bad footer", file);
        goto done;
    }

    rc = 1;

done:
    sodium_memzero(line, sizeof(line));
    fclose(f);
    return rc;
}

static int pem_write(const char *file, const char *header, const char *footer, const unsigned char *key, size_t key_len,
                     mode_t mode) {
    int fd = open(file, O_WRONLY | O_CREAT | O_EXCL, mode);
    if (fd < 0) {
        ERROR("Failed to create %s", file);
        return 0;
    }

    FILE *f = fdopen(fd, "w");
    if (!f) {
        ERROR("Failed to open stream for %s", file);
        close(fd);
        unlink(file);
        return 0;
    }

    char b64[PEM_LINE_MAX];
    sodium_bin2base64(b64, sizeof(b64), key, key_len, B64_VARIANT);

    int ok = fprintf(f, "%s\n%s\n%s\n", header, b64, footer) > 0;
    if (fclose(f) != 0)
        ok = 0;

    sodium_memzero(b64, sizeof(b64));

    if (!ok) {
        ERROR("Failed to write %s", file);
        unlink(file);
        return 0;
    }

    return 1;
}

static int pem_path(char *out, size_t size, const char *dir, const char *file) {
    int n = snprintf(out, size, "%s/%s", dir, file);
    if (n < 0 || (size_t)n >= size) {
        ERROR("Key path for %s is too long", file);
        return 0;
    }

    return 1;
}

int key_pair_load(const char *path, struct Node *node) {
    if (!path || !node) {
        ERROR("Cannot load ed25519 key pair. path or node is null");
        return 0;
    }

    char sec_path[PATH_MAX];
    char pub_path[PATH_MAX];

    if (!pem_path(sec_path, sizeof(sec_path), path, SEC_FILE) || !pem_path(pub_path, sizeof(pub_path), path, PUB_FILE))
        return 0;

    sodium_memzero(node->long_term_public_key, sizeof(node->long_term_public_key));
    sodium_memzero(node->long_term_private_key, sizeof(node->long_term_private_key));

    int has_sec = pem_read(sec_path, SEC_HEADER, SEC_FOOTER, node->long_term_private_key, sizeof(node->long_term_private_key));
    int has_pub = pem_read(pub_path, PUB_HEADER, PUB_FOOTER, node->long_term_public_key, sizeof(node->long_term_public_key));

    if (has_sec < 0 || has_pub < 0) {
        sodium_memzero(node, sizeof(*node));
        return 0;
    }

    if (has_sec != has_pub) {
        ERROR("Incomplete key pair in %s: only %s is present", path, has_sec ? SEC_FILE : PUB_FILE);
        sodium_memzero(node, sizeof(*node));
        return 0;
    }

    if (has_sec) {
        unsigned char derived[crypto_sign_PUBLICKEYBYTES];
        if (crypto_sign_ed25519_sk_to_pk(derived, node->long_term_private_key) != 0 ||
            sodium_memcmp(derived, node->long_term_public_key, sizeof(derived)) != 0) {
            ERROR("%s does not match %s", PUB_FILE, SEC_FILE);
            sodium_memzero(node, sizeof(*node));
            return 0;
        }

        INFO("Loaded ed25519 key pair from %s", path);
        return 1;
    }

    if (mkdir(path, 0700) != 0 && errno != EEXIST) {
        ERROR("Failed to create key directory %s", path);
        return 0;
    }

    if (crypto_sign_keypair(node->long_term_public_key, node->long_term_private_key) != 0) {
        ERROR("Failed to generate ed25519 key pair");
        sodium_memzero(node, sizeof(*node));
        return 0;
    }

    if (!pem_write(sec_path, SEC_HEADER, SEC_FOOTER, node->long_term_private_key, sizeof(node->long_term_private_key), 0600) ||
        !pem_write(pub_path, PUB_HEADER, PUB_FOOTER, node->long_term_public_key, sizeof(node->long_term_public_key), 0644)) {
        unlink(sec_path);
        sodium_memzero(node, sizeof(*node));
        return 0;
    }

    INFO("Generated ed25519 key pair in %s", path);
    return 1;
}
