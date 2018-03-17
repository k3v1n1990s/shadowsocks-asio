
#include <sodium.h>
#include <boost/endian/buffers.hpp>

#include <common_utils/common.h>

#include "crypto_utils/crypto.h"
#include "crypto_utils/aead.h"

static const CryptoContextGeneratorRegister<Chacha20Poly1305Ietf> kReg("chacha20-ietf-poly1305");

int Chacha20Poly1305Ietf::CipherEncrypt(
                void *c, size_t *clen,
                const uint8_t *m, size_t mlen,
                const uint8_t *ad, size_t adlen,
                const uint8_t *n, const uint8_t *k
    ) {
    int ret;
    unsigned long long clenll;
    ret = crypto_aead_chacha20poly1305_ietf_encrypt(
                    (uint8_t *)c, &clenll,
                    m, mlen, ad, adlen,
                    nullptr, n, k
          );
    *clen = (size_t)clenll;
    return ret;
}

int Chacha20Poly1305Ietf::CipherDecrypt(
                void *m, size_t *mlen,
                const uint8_t *c, size_t clen,
                const uint8_t *ad, size_t adlen,
                const uint8_t *n, const uint8_t *k
    ) {
    int ret;
    unsigned long long mlenll;
    ret = crypto_aead_chacha20poly1305_ietf_decrypt(
                    (uint8_t *)m, &mlenll,
                    nullptr,
                    c, clen, ad, adlen,
                    n, k
          );
    *mlen = (size_t)mlenll;
    return ret;
}
