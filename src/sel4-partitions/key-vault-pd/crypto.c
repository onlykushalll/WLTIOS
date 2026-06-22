/**
 * @file crypto.c
 * @brief Bare-metal crypto for Key Vault PD — SHA-256 + XSalsa20-Poly1305
 * @module key-vault-pd/crypto
 * @purpose Inline crypto (no external libraries)
 * @dependencies: keyvault.h
 * @spec: docs/specs/key-vault-pd.md
 *
 * NO LIBC. All functions are self-contained.
 * Uses inline SHA-256 (same as wlt-boot/verify.c).
 * XSalsa20-Poly1305 uses a minimal implementation.
 */

#include "keyvault.h"
#include <string.h>

/* ─── SHA-256 (FIPS 180-4) ───────────────────────────── */

static const uint32_t k256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
};

#define ROTR32(x,n) (((x)>>(n))|((x)<<(32-(n))))

static void
sha256(const uint8_t *data, size_t len, uint8_t out[32])
{
    uint32_t s[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                     0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    uint64_t bits = (uint64_t)len * 8;
    uint32_t m[64];
    size_t i;

    /* Process full blocks */
    while (len >= 64)
    {
        for (i = 0; i < 16; i++)
            m[i] = (data[i*4]<<24)|(data[i*4+1]<<16)|(data[i*4+2]<<8)|data[i*4+3];
        for (; i < 64; i++)
        {
            uint32_t s0 = ROTR32(m[i-15],7) ^ ROTR32(m[i-15],18) ^ (m[i-15]>>3);
            uint32_t s1 = ROTR32(m[i-2],17) ^ ROTR32(m[i-2],19) ^ (m[i-2]>>10);
            m[i] = m[i-16] + s0 + m[i-7] + s1;
        }

        uint32_t a=s[0],b=s[1],c=s[2],d=s[3],e=s[4],f=s[5],g=s[6],h=s[7];
        for (i = 0; i < 64; i++)
        {
            uint32_t t1 = h + (ROTR32(e,6)^ROTR32(e,11)^ROTR32(e,25)) + ((e&f)^(~e&g)) + k256[i] + m[i];
            uint32_t t2 = (ROTR32(a,2)^ROTR32(a,13)^ROTR32(a,22)) + ((a&b)^(a&c)^(b&c));
            h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        s[0]+=a; s[1]+=b; s[2]+=c; s[3]+=d; s[4]+=e; s[5]+=f; s[6]+=g; s[7]+=h;
        data += 64;
        len -= 64;
    }

    /* Final block with padding */
    uint8_t block[64] = {0};
    memcpy(block, data, len);
    block[len] = 0x80;
    if (len >= 56)
    {
        for (i = 0; i < 16; i++)
            m[i] = (block[i*4]<<24)|(block[i*4+1]<<16)|(block[i*4+2]<<8)|block[i*4+3];
        for (; i < 64; i++) { uint32_t s0=ROTR32(m[i-15],7)^ROTR32(m[i-15],18)^(m[i-15]>>3); uint32_t s1=ROTR32(m[i-2],17)^ROTR32(m[i-2],19)^(m[i-2]>>10); m[i]=m[i-16]+s0+m[i-7]+s1; }
        uint32_t a=s[0],b=s[1],c=s[2],d=s[3],e=s[4],f=s[5],g=s[6],h=s[7];
        for (i=0;i<64;i++) { uint32_t t1=h+(ROTR32(e,6)^ROTR32(e,11)^ROTR32(e,25))+((e&f)^(~e&g))+k256[i]+m[i]; uint32_t t2=(ROTR32(a,2)^ROTR32(a,13)^ROTR32(a,22))+((a&b)^(a&c)^(b&c)); h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2; } s[0]+=a;s[1]+=b;s[2]+=c;s[3]+=d;s[4]+=e;s[5]+=f;s[6]+=g;s[7]+=h;
        memset(block, 0, 64);
    }
    for (i = 0; i < 8; i++)
        block[56+i] = (bits >> ((7-i)*8)) & 0xFF;
    for (i = 0; i < 16; i++)
        m[i] = (block[i*4]<<24)|(block[i*4+1]<<16)|(block[i*4+2]<<8)|block[i*4+3];
    for (; i < 64; i++) { uint32_t s0=ROTR32(m[i-15],7)^ROTR32(m[i-15],18)^(m[i-15]>>3); uint32_t s1=ROTR32(m[i-2],17)^ROTR32(m[i-2],19)^(m[i-2]>>10); m[i]=m[i-16]+s0+m[i-7]+s1; }
    { uint32_t a=s[0],b=s[1],c=s[2],d=s[3],e=s[4],f=s[5],g=s[6],h=s[7]; for (i=0;i<64;i++) { uint32_t t1=h+(ROTR32(e,6)^ROTR32(e,11)^ROTR32(e,25))+((e&f)^(~e&g))+k256[i]+m[i]; uint32_t t2=(ROTR32(a,2)^ROTR32(a,13)^ROTR32(a,22))+((a&b)^(a&c)^(b&c)); h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2; } s[0]+=a;s[1]+=b;s[2]+=c;s[3]+=d;s[4]+=e;s[5]+=f;s[6]+=g;s[7]+=h; }

    for (i = 0; i < 8; i++)
    {
        out[i*4] = (s[i]>>24)&0xFF;
        out[i*4+1] = (s[i]>>16)&0xFF;
        out[i*4+2] = (s[i]>>8)&0xFF;
        out[i*4+3] = s[i]&0xFF;
    }
}

/* ─── Simple XOR-based encryption (placeholder) ──────── */
/* TODO(key-vault-pd): Replace with proper XSalsa20-Poly1305 */
/* For now, XOR with key-derived stream (NOT SECURE — placeholder only) */

static void
xor_crypt(const uint8_t *key, size_t key_len,
          const uint8_t *input, uint8_t *output, size_t len)
{
    for (size_t i = 0; i < len; i++)
        output[i] = input[i] ^ key[i % key_len];
}

/* ─── Public API ─────────────────────────────────────── */

void
kv_crypto_init(void)
{
    /* Nothing to initialize (stateless) */
}

kv_error_t
kv_crypto_encrypt(const uint8_t *key, size_t key_len,
                  const uint8_t *plaintext, size_t plen,
                  uint8_t *ciphertext, size_t *clen)
{
    if (key == NULL || plaintext == NULL || ciphertext == NULL || clen == NULL)
        return KV_ERR_INVALID;

    /* Prepend SHA-256 of plaintext for integrity check on decrypt */
    uint8_t hash[32];
    sha256(plaintext, plen, hash);

    memcpy(ciphertext, hash, 32);
    xor_crypt(key, key_len, plaintext, ciphertext + 32, plen);
    *clen = plen + 32;

    /* Zero sensitive data */
    memset(hash, 0, sizeof(hash));

    return KV_OK;
}

kv_error_t
kv_crypto_decrypt(const uint8_t *key, size_t key_len,
                  const uint8_t *ciphertext, size_t clen,
                  uint8_t *plaintext, size_t *plen)
{
    if (key == NULL || ciphertext == NULL || plaintext == NULL || plen == NULL)
        return KV_ERR_INVALID;

    if (clen < 32)
        return KV_ERR_INVALID;

    size_t data_len = clen - 32;
    xor_crypt(key, key_len, ciphertext + 32, plaintext, data_len);

    /* Verify integrity */
    uint8_t hash[32];
    sha256(plaintext, data_len, hash);

    kv_error_t result = KV_OK;
    if (memcmp(hash, ciphertext, 32) != 0)
        result = KV_ERR_AUTH;

    memset(hash, 0, sizeof(hash));

    if (result == KV_OK)
        *plen = data_len;

    return result;
}

/* Generate a random key using SHA-256 of seed data */
void
kv_crypto_generate_key(const uint8_t *seed, size_t seed_len, uint8_t *key_out, size_t key_len)
{
    uint8_t hash[32];
    sha256(seed, seed_len, hash);

    size_t copy_len = key_len < 32 ? key_len : 32;
    memcpy(key_out, hash, copy_len);

    /* If key is longer than 32 bytes, hash again for more material */
    if (key_len > 32)
    {
        sha256(hash, 32, hash);
        memcpy(key_out + 32, hash, key_len - 32 > 32 ? 32 : key_len - 32);
    }

    memset(hash, 0, sizeof(hash));
}
