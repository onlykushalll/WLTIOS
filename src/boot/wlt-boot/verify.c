/**
 * @file verify.c
 * @brief Signature verification implementation
 * @module wlt-boot/verify
 * @purpose SHA-256 hashing for seL4 kernel verification
 * @dependencies: verify.h, console.h
 * @tested_by: tests/boot/verify_test.c
 * @spec: docs/specs/wlt-boot.md
 * @adr: docs/adr/01-uefi-vs-bios.md (verified boot)
 *
 * SHA-256 implementation based on FIPS 180-4.
 * Public domain — no license restrictions.
 */

#include "verify.h"
#include "console.h"
#include <string.h>

/* ─── SHA-256 Constants ──────────────────────────────── */

static const uint32_t k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

#define SHA256_BLOCK_SIZE 64
#define SHA256_DIGEST_SIZE 32

/* SHA-256 context */
typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint32_t datalen;
    uint8_t data[SHA256_BLOCK_SIZE];
} sha256_ctx_t;

/* ─── SHA-256 Internal Functions ─────────────────────── */

#define ROTR(x, n)  (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z)  (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x)  (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x)  (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

static void
sha256_transform(sha256_ctx_t *ctx, const uint8_t *data)
{
    uint32_t a, b, c, d, e, f, g, h, t1, t2;
    uint32_t m[64];
    int i;

    /* Build message schedule */
    for (i = 0; i < 16; i++)
    {
        m[i] = ((uint32_t)data[i * 4] << 24) |
               ((uint32_t)data[i * 4 + 1] << 16) |
               ((uint32_t)data[i * 4 + 2] << 8) |
               ((uint32_t)data[i * 4 + 3]);
    }
    for (i = 16; i < 64; i++)
    {
        m[i] = SIG1(m[i - 2]) + m[i - 7] + SIG0(m[i - 15]) + m[i - 16];
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for (i = 0; i < 64; i++)
    {
        t1 = h + EP1(e) + CH(e, f, g) + k[i] + m[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void
sha256_init(sha256_ctx_t *ctx)
{
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

static void
sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len)
{
    uint32_t i;

    for (i = 0; i < len; i++)
    {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == SHA256_BLOCK_SIZE)
        {
            sha256_transform(ctx, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

static void
sha256_final(sha256_ctx_t *ctx, uint8_t *hash)
{
    uint32_t i;

    /* Pad the last block */
    i = ctx->datalen;

    if (ctx->datalen < 56)
    {
        ctx->data[i++] = 0x80;
        while (i < 56)
            ctx->data[i++] = 0;
    }
    else
    {
        ctx->data[i++] = 0x80;
        while (i < 64)
            ctx->data[i++] = 0;
        sha256_transform(ctx, ctx->data);
        memset(ctx->data, 0, 56);
    }

    /* Append length */
    ctx->bitlen += (uint64_t)ctx->datalen * 8;
    ctx->data[63] = (uint8_t)(ctx->bitlen);
    ctx->data[62] = (uint8_t)(ctx->bitlen >> 8);
    ctx->data[61] = (uint8_t)(ctx->bitlen >> 16);
    ctx->data[60] = (uint8_t)(ctx->bitlen >> 24);
    ctx->data[59] = (uint8_t)(ctx->bitlen >> 32);
    ctx->data[58] = (uint8_t)(ctx->bitlen >> 40);
    ctx->data[57] = (uint8_t)(ctx->bitlen >> 48);
    ctx->data[56] = (uint8_t)(ctx->bitlen >> 56);
    sha256_transform(ctx, ctx->data);

    /* Output hash (big-endian) */
    for (i = 0; i < 4; i++)
    {
        hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0xFF;
        hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0xFF;
        hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0xFF;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0xFF;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0xFF;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0xFF;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0xFF;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0xFF;
    }
}

/* ─── Public API ─────────────────────────────────────── */

void
wlt_verify_sha256(const uint8_t *data, size_t size, uint8_t hash[32])
{
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, size);
    sha256_final(&ctx, hash);
}

wlt_boot_error_t
wlt_verify_kernel(const uint8_t *kernel_data,
                   size_t kernel_size,
                   const uint8_t *sig_data,
                   size_t sig_size)
{
    uint8_t computed_hash[32];

    if (kernel_data == NULL || kernel_size == 0)
    {
        wlt_console_error("Invalid kernel data");
        return WLT_BOOT_ERR_INVALID_ARG;
    }

    if (sig_data == NULL || sig_size < 32)
    {
        wlt_console_error("Invalid signature data");
        return WLT_BOOT_ERR_INVALID_ARG;
    }

    /* Compute SHA-256 hash of kernel */
    wlt_verify_sha256(kernel_data, kernel_size, computed_hash);

    /* Compare with expected hash from signature file */
    /* For now, sig_data is expected to be the raw 32-byte SHA-256 hash */
    /* TODO(wlt-boot): Add Ed25519 signature verification (not just hash comparison) */
    if (memcmp(computed_hash, sig_data, 32) != 0)
    {
        wlt_console_error("Kernel signature mismatch!");
        wlt_console_printf("  Expected: %x... (first 4 bytes)\n",
                           *(uint32_t *)sig_data);
        wlt_console_printf("  Got:      %x... (first 4 bytes)\n",
                           *(uint32_t *)computed_hash);
        return WLT_BOOT_ERR_SIGNATURE;
    }

    return WLT_BOOT_OK;
}

wlt_boot_error_t
wlt_verify_ed25519(const uint8_t *data, size_t data_size,
                    const uint8_t sig[64],
                    const uint8_t pubkey[32])
{
    /* SECURITY FIX: this previously returned WLT_BOOT_OK unconditionally —
     * a fail-open bug. If anything called wlt_verify_ed25519() expecting
     * genuine verification, it would accept ANY input as valid.
     *
     * Now FAILS CLOSED: returns WLT_BOOT_ERR_SIGNATURE until a real
     * Ed25519 implementation (monocypher — single-file, no deps, easy to
     * vendor for UEFI) is integrated. This means:
     *   - If the boot config requests Ed25519 verification, boot REFUSES
     *   - If the boot config uses SHA-256 hash comparison (current default),
     *     this function is not called + boot proceeds via wlt_verify_kernel
     *
     * TRUST BOUNDARY: the boot signing key is SEPARATE from the package
     * signing key (wlt-update). Rationale: if the package key is compromised
     * (malicious update), the boot key remains safe, so the attacker cannot
     * boot a tampered kernel. Different threat models:
     *   boot key = protects against evil-maid (physical access)
     *   package key = protects against MITM (malicious updates)
     *
     * The boot pubkey (32 bytes Ed25519) should live at:
     *   /boot/keys/boot-pubkey.bin (on the ESP)
     * NOT the same file as /etc/wlt/keys/update-pubkey.bin (package key).
     *
     * Current wlt_verify_kernel() uses raw SHA-256 hash comparison — this
     * provides ZERO protection against deliberate tampering (an attacker
     * can recompute a matching hash). It only catches accidental
     * corruption. Real verified boot requires Ed25519 (asymmetric —
     * the attacker cannot forge a signature without the private key).
     * Integrating monocypher is the tracked follow-up for real verified boot.
     */
    (void)data;
    (void)data_size;
    (void)sig;
    (void)pubkey;

    wlt_console_error("Ed25519 verification not implemented — REFUSING (fail-closed)");
    wlt_console_error("Use SHA-256 hash comparison (wlt_verify_kernel) or");
    wlt_console_error("integrate monocypher for real Ed25519 verified boot.");
    return WLT_BOOT_ERR_SIGNATURE;
}

