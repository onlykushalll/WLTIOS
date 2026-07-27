/*
 * boot_verifier_pd.c — Boot Verifier Protection Domain
 *
 * Bare seL4 component (no Linux, no filesystem, no sockets).
 * Verifies system integrity via SHA-256 hashing + comparison.
 *
 * Real verification: reads [32-byte expected_hash][data...] from
 * request_dp, computes SHA-256 of data, compares, responds OK or
 * TAMPERED based on the real result.
 *
 * Two-directions proof: matching hash → OK, mismatched → TAMPERED.
 */
#include <camkes.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define BOOT_VERIFY_OK      "OK"
#define BOOT_VERIFY_TAMPERED "TAMPERED"
#define BOOT_VERIFY_ERROR   "ERROR"

/* ─── SHA-256 (FIPS 180-4, public domain) ───────────────── */
#define SHA256_BLOCK_SIZE 64
#define SHA256_DIGEST_SIZE 32

typedef struct {
    uint32_t state[8]; uint64_t bitlen; uint32_t datalen; uint8_t data[SHA256_BLOCK_SIZE];
} sha256_ctx_t;

static const uint32_t sha_k[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,
    0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,
    0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,
    0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,
    0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,
    0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
};

#define ROTR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define CH(x,y,z) (((x)&(y))^(~(x)&(z)))
#define MAJ(x,y,z) (((x)&(y))^((x)&(z))^((y)&(z)))
#define EP0(x) (ROTR(x,2)^ROTR(x,13)^ROTR(x,22))
#define EP1(x) (ROTR(x,6)^ROTR(x,11)^ROTR(x,25))
#define SIG0(x) (ROTR(x,7)^ROTR(x,18)^((x)>>3))
#define SIG1(x) (ROTR(x,17)^ROTR(x,19)^((x)>>10))

static void sha256_transform(sha256_ctx_t *ctx, const uint8_t *data) {
    uint32_t a,b,c,d,e,f,g,h,t1,t2,m[64]; int i;
    for (i=0;i<16;i++) m[i]=((uint32_t)data[i*4]<<24)|((uint32_t)data[i*4+1]<<16)|((uint32_t)data[i*4+2]<<8)|data[i*4+3];
    for (i=16;i<64;i++) m[i]=SIG1(m[i-2])+m[i-7]+SIG0(m[i-15])+m[i-16];
    a=ctx->state[0];b=ctx->state[1];c=ctx->state[2];d=ctx->state[3];
    e=ctx->state[4];f=ctx->state[5];g=ctx->state[6];h=ctx->state[7];
    for (i=0;i<64;i++) {
        t1=h+EP1(e)+CH(e,f,g)+sha_k[i]+m[i]; t2=EP0(a)+MAJ(a,b,c);
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    ctx->state[0]+=a;ctx->state[1]+=b;ctx->state[2]+=c;ctx->state[3]+=d;
    ctx->state[4]+=e;ctx->state[5]+=f;ctx->state[6]+=g;ctx->state[7]+=h;
}

static void sha256_init(sha256_ctx_t *ctx) {
    ctx->state[0]=0x6a09e667;ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372;ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f;ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab;ctx->state[7]=0x5be0cd19;
    ctx->bitlen=0;ctx->datalen=0;
}

static void sha256_update(sha256_ctx_t *ctx, const uint8_t *data, size_t len) {
    for (size_t i=0;i<len;i++) {
        ctx->data[ctx->datalen++]=data[i];
        if (ctx->datalen==SHA256_BLOCK_SIZE) {
            sha256_transform(ctx,ctx->data); ctx->bitlen+=SHA256_BLOCK_SIZE*8; ctx->datalen=0;
        }
    }
}

static void sha256_final(sha256_ctx_t *ctx, uint8_t hash[32]) {
    uint32_t i=ctx->datalen; ctx->data[i++]=0x80;
    if (i>56) { while(i<64) ctx->data[i++]=0; sha256_transform(ctx,ctx->data); i=0; }
    while(i<56) ctx->data[i++]=0;
    ctx->bitlen+=ctx->datalen*8;
    ctx->data[56]=(uint8_t)(ctx->bitlen>>56); ctx->data[57]=(uint8_t)(ctx->bitlen>>48);
    ctx->data[58]=(uint8_t)(ctx->bitlen>>40); ctx->data[59]=(uint8_t)(ctx->bitlen>>32);
    ctx->data[60]=(uint8_t)(ctx->bitlen>>24); ctx->data[61]=(uint8_t)(ctx->bitlen>>16);
    ctx->data[62]=(uint8_t)(ctx->bitlen>>8); ctx->data[63]=(uint8_t)(ctx->bitlen);
    sha256_transform(ctx,ctx->data);
    for (i=0;i<4;i++) {
        hash[i]=(uint8_t)(ctx->state[0]>>(24-i*8));
        hash[i+4]=(uint8_t)(ctx->state[1]>>(24-i*8));
        hash[i+8]=(uint8_t)(ctx->state[2]>>(24-i*8));
        hash[i+12]=(uint8_t)(ctx->state[3]>>(24-i*8));
        hash[i+16]=(uint8_t)(ctx->state[4]>>(24-i*8));
        hash[i+20]=(uint8_t)(ctx->state[5]>>(24-i*8));
        hash[i+24]=(uint8_t)(ctx->state[6]>>(24-i*8));
        hash[i+28]=(uint8_t)(ctx->state[7]>>(24-i*8));
    }
}

/* ─── Boot Verifier Control Loop ─────────────────────────── */
static void boot_verifier_run(void) {
    printf("[boot-verifier] alive\n");
    printf("[boot-verifier] Waiting for verify request from Linux VM...\n");
    for (;;) {
        verify_request_wait();
        uint8_t *req = (uint8_t *)request_dp;
        char *resp = (char *)response_dp;
        /* Request: [32-byte expected_hash][data to verify...] */
        uint8_t *expected_hash = req;
        uint8_t *data = req + 32;
        size_t data_size = 4096 - 32;
        /* Compute SHA-256 of data */
        sha256_ctx_t ctx; sha256_init(&ctx); sha256_update(&ctx, data, data_size);
        uint8_t computed[32]; sha256_final(&ctx, computed);
        /* Compare */
        if (memcmp(expected_hash, computed, 32) == 0) {
            printf("[boot-verifier] verify: hash MATCH → OK\n");
            strncpy(resp, BOOT_VERIFY_OK, 4096);
        } else {
            printf("[boot-verifier] verify: hash MISMATCH → TAMPERED\n");
            strncpy(resp, BOOT_VERIFY_TAMPERED, 4096);
        }
        verify_done_emit();
    }
}

int main(void) { boot_verifier_run(); return 0; }
