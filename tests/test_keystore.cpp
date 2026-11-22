// SPDX-License-Identifier: AGPL-3.0-or-later
// T-S12-key (keychain part, Spec S12.6): save, load, replace and clear a key under a test account (never
// the synth's own "assistant"). Where the keychain cannot be used (Linux without libsecret or without a
// running Secret Service, a locked CI keychain) the errors must come back as text, and the test says so.
#include "app/KeyStore.h"
#include <cstdio>

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++failures; } } while (0)

int main() {
    using namespace lmw;
    const std::string account = "lmw-synth-test";
    std::string e;
    std::printf("keychain available: %s\n", keystore::available() ? "yes" : "no");
    if (!keystore::save("sk-ant-test-1", e, account)) {
        std::printf("keychain not usable here (%s): only the error path is checked\n", e.c_str());
        CHECK(!e.empty());
        std::string e2;
        CHECK(!keystore::load(e2, account).has_value());
        std::printf(failures ? "%d FAILED\n" : "T-S12-key (keychain): passed (error path)\n", failures);
        return failures ? 1 : 0;
    }
    auto k = keystore::load(e, account);
    CHECK(k && *k == "sk-ant-test-1" && e.empty());
    CHECK(keystore::save("sk-ant-test-2", e, account));                 // replaces
    k = keystore::load(e, account);
    CHECK(k && *k == "sk-ant-test-2");
    CHECK(keystore::clear(e, account));
    k = keystore::load(e, account);
    CHECK(!k && e.empty());                                              // gone, not an error
    CHECK(keystore::clear(e, account));                                  // clearing twice is fine
    std::printf(failures ? "%d FAILED\n" : "T-S12-key (keychain): all passed (round trip)\n", failures);
    return failures ? 1 : 0;
}
