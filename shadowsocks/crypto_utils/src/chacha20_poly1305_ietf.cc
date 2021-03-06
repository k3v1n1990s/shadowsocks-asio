
#include <sodium.h>

#include "crypto_utils/crypto.h"
#include "crypto_utils/aead.h"

class Chacha20Poly1305Ietf final : public AeadCipher<32, 12, 16> {
public:
    Chacha20Poly1305Ietf(std::vector<uint8_t> master_key)
        : AeadCipher(std::move(master_key)) {
    }

    ~Chacha20Poly1305Ietf() { }

private:
    int CipherEncrypt(
                void *c, size_t *clen,
                const uint8_t *m, size_t mlen,
                const uint8_t *ad, size_t adlen
        );

    int CipherDecrypt(
                void *m, size_t *mlen,
                const uint8_t *c, size_t clen,
                const uint8_t *ad, size_t adlen
        );
};

int Chacha20Poly1305Ietf::CipherEncrypt(
                void *c, size_t *clen,
                const uint8_t *m, size_t mlen,
                const uint8_t *ad, size_t adlen
    ) {
    int ret;
    unsigned long long clenll;
    ret = crypto_aead_chacha20poly1305_ietf_encrypt(
                    (uint8_t *)c, &clenll,
                    m, mlen, ad, adlen,
                    nullptr, nonce_.data(), key_.data()
          );
    *clen = (size_t)clenll;
    return ret;
}

int Chacha20Poly1305Ietf::CipherDecrypt(
                void *m, size_t *mlen,
                const uint8_t *c, size_t clen,
                const uint8_t *ad, size_t adlen
    ) {
    int ret;
    unsigned long long mlenll;
    ret = crypto_aead_chacha20poly1305_ietf_decrypt(
                    (uint8_t *)m, &mlenll,
                    nullptr,
                    c, clen, ad, adlen,
                    nonce_.data(), key_.data()
          );
    *mlen = (size_t)mlenll;
    return ret;
}

static const CryptoContextGeneratorRegister<Chacha20Poly1305Ietf> kReg("chacha20-ietf-poly1305");

