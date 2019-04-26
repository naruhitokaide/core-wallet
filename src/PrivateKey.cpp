// Copyright © 2017-2019 Trust Wallet.
//
// This file is part of Trust. The full Trust copyright notice, including
// terms governing use, modification, and redistribution, is contained in the
// file LICENSE at the root of the source code distribution tree.

#include "PrivateKey.h"

#include "PublicKey.h"

#include <TrezorCrypto/ecdsa.h>
#include <TrezorCrypto/nist256p1.h>
#include <TrezorCrypto/rand.h>
#include <TrezorCrypto/secp256k1.h>

using namespace TW;

PrivateKey::~PrivateKey() {
    std::fill(bytes.begin(), bytes.end(), 0);
}

PublicKey PrivateKey::getPublicKey(PublicKeyType type) const {
    Data result;
    switch (type) {
    case PublicKeyType::secp256k1:
        result.resize(PublicKey::secp256k1Size);
        ecdsa_get_public_key33(&secp256k1, bytes.data(), result.data());
        break;
    case PublicKeyType::secp256k1Extended:
        result.resize(PublicKey::secp256k1ExtendedSize);
        ecdsa_get_public_key65(&secp256k1, bytes.data(), result.data());
        break;
    case PublicKeyType::ed25519:
        result.resize(PublicKey::ed25519Size);
        result[0] = 1;
        ed25519_publickey(bytes.data(), result.data() + 1);
        break;
    case PublicKeyType::nist256p1:
        result.resize(PublicKey::secp256k1Size);
        ecdsa_get_public_key33(&nist256p1, bytes.data(), result.data());
        break;
    }
    return PublicKey(result);
}

Data PrivateKey::sign(const Data& digest, TWCurve curve) const {
    Data result;
    bool success = true;
    switch (curve) {
    case TWCurveSECP256k1: {
        result.resize(65);
        success = ecdsa_sign_digest(&secp256k1, bytes.data(), digest.data(), result.data(),
                                    result.data() + 64, nullptr) == 0;
    } break;
    case TWCurveED25519: {
        result.resize(64);
        const auto publicKey = getPublicKey(PublicKeyType::ed25519);
        ed25519_sign(digest.data(), digest.size(), bytes.data(), publicKey.bytes.data() + 1,
                     result.data());
    } break;
    case TWCurveNIST256p1: {
        result.resize(65);
        success = ecdsa_sign_digest(&nist256p1, bytes.data(), digest.data(), result.data(),
                                    result.data() + 64, nullptr) == 0;
    } break;
    }

    if (!success) {
        return {};
    }
    return result;
}

Data PrivateKey::sign(const Data& digest, TWCurve curve, int(*canonicalChecker)(uint8_t by, uint8_t sig[64])) const {
    Data result;
    bool success = false;
    switch (curve) {
    case TWCurveSECP256k1: {
        result.resize(65);
        success = ecdsa_sign_digest(&secp256k1, bytes.data(), digest.data(), result.data() + 1,
                                    result.data(), canonicalChecker) == 0;
    } break;
    case TWCurveED25519: // not supported
        break;
    case TWCurveNIST256p1: {
        result.resize(65);
        success = ecdsa_sign_digest(&nist256p1, bytes.data(), digest.data(), result.data() + 1,
                                    result.data(), canonicalChecker) == 0;
    } break;
    }

    if (!success) {
        return {};
    }

    // graphene adds 31 to the recovery id
    result[0] += 31;
    return result;
}

Data PrivateKey::signAsDER(const Data& digest, TWCurve curve) const {
    Data sig(64);
    bool success =
        ecdsa_sign_digest(&secp256k1, bytes.data(), digest.data(), sig.data(), nullptr, nullptr) == 0;
    if (!success) {
        return {};
    }

    std::array<uint8_t, 72> resultBytes;
    size_t size = ecdsa_sig_to_der(sig.data(), resultBytes.data());

    auto result = Data{};
    std::copy(resultBytes.begin(), resultBytes.begin() + size, std::back_inserter(result));
    return result;
}
