// SPDX-License-Identifier: AGPL-3.0-or-later
#include "KeyStore.h"

#if defined(_WIN32)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <wincred.h>
 #include <string>
#elif defined(__APPLE__)
 #include <Security/Security.h>
#else
 #include <dlfcn.h>
#endif

namespace lmw::keystore {

#if defined(_WIN32)

namespace {
std::wstring target(const std::string& account) { return L"LMW-Synth/" + std::wstring(account.begin(), account.end()); }
std::string lastError(const char* what) { return std::string(what) + " failed (Windows error " + std::to_string(GetLastError()) + ")"; }
} // namespace

bool available() { return true; }

std::optional<std::string> load(std::string& error, const std::string& account) {
    error.clear();
    PCREDENTIALW cred = nullptr;
    if (!CredReadW(target(account).c_str(), CRED_TYPE_GENERIC, 0, &cred)) {
        if (GetLastError() != ERROR_NOT_FOUND) error = lastError("reading the key from the Credential Manager");
        return std::nullopt;
    }
    std::string key(reinterpret_cast<const char*>(cred->CredentialBlob), cred->CredentialBlobSize);
    CredFree(cred);
    return key;
}

bool save(const std::string& key, std::string& error, const std::string& account) {
    auto name = target(account);
    CREDENTIALW cred{};
    cred.Type = CRED_TYPE_GENERIC;
    cred.TargetName = name.data();
    cred.CredentialBlobSize = static_cast<DWORD>(key.size());
    cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(key.data()));
    cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
    cred.UserName = const_cast<wchar_t*>(L"assistant");
    if (!CredWriteW(&cred, 0)) { error = lastError("saving the key in the Credential Manager"); return false; }
    return true;
}

bool clear(std::string& error, const std::string& account) {
    if (!CredDeleteW(target(account).c_str(), CRED_TYPE_GENERIC, 0) && GetLastError() != ERROR_NOT_FOUND) {
        error = lastError("removing the key from the Credential Manager");
        return false;
    }
    return true;
}

#elif defined(__APPLE__)

namespace {
CFMutableDictionaryRef query(const std::string& account) {
    auto* q = CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(q, kSecClass, kSecClassGenericPassword);
    CFDictionarySetValue(q, kSecAttrService, CFSTR("LMW-Synth"));
    auto* a = CFStringCreateWithCString(nullptr, account.c_str(), kCFStringEncodingUTF8);
    CFDictionarySetValue(q, kSecAttrAccount, a);
    CFRelease(a);
    return q;
}
std::string statusText(const char* what, OSStatus s) { return std::string(what) + " failed (Keychain error " + std::to_string(s) + ")"; }
} // namespace

bool available() { return true; }

std::optional<std::string> load(std::string& error, const std::string& account) {
    error.clear();
    auto* q = query(account);
    CFDictionarySetValue(q, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(q, kSecMatchLimit, kSecMatchLimitOne);
    CFTypeRef data = nullptr;
    const OSStatus s = SecItemCopyMatching(q, &data);
    CFRelease(q);
    if (s == errSecItemNotFound) return std::nullopt;
    if (s != errSecSuccess || data == nullptr) { error = statusText("reading the key from the Keychain", s); return std::nullopt; }
    auto* d = static_cast<CFDataRef>(data);
    std::string key(reinterpret_cast<const char*>(CFDataGetBytePtr(d)), static_cast<std::size_t>(CFDataGetLength(d)));
    CFRelease(data);
    return key;
}

bool save(const std::string& key, std::string& error, const std::string& account) {
    std::string ignored;
    clear(ignored, account);
    auto* q = query(account);
    auto* d = CFDataCreate(nullptr, reinterpret_cast<const UInt8*>(key.data()), static_cast<CFIndex>(key.size()));
    CFDictionarySetValue(q, kSecValueData, d);
    const OSStatus s = SecItemAdd(q, nullptr);
    CFRelease(d);
    CFRelease(q);
    if (s != errSecSuccess) { error = statusText("saving the key in the Keychain", s); return false; }
    return true;
}

bool clear(std::string& error, const std::string& account) {
    auto* q = query(account);
    const OSStatus s = SecItemDelete(q);
    CFRelease(q);
    if (s != errSecSuccess && s != errSecItemNotFound) { error = statusText("removing the key from the Keychain", s); return false; }
    return true;
}

#else   // Linux and other Unix: libsecret (Secret Service: GNOME Keyring, KWallet ...)

namespace {
// The libsecret / GLib types we use, declared here so no headers are needed (layouts as in libsecret 0.20).
struct GError { unsigned domain; int code; char* message; };
struct SecretSchemaAttribute { const char* name; int type; };
struct SecretSchema {
    const char* name;
    int flags;
    SecretSchemaAttribute attributes[32];
    int reserved;
    void* reserved1; void* reserved2; void* reserved3; void* reserved4; void* reserved5; void* reserved6; void* reserved7;
};
using StoreFn = int (*)(const SecretSchema*, const char*, const char*, const char*, void*, GError**, ...);
using LookupFn = char* (*)(const SecretSchema*, void*, GError**, ...);
using ClearFn = int (*)(const SecretSchema*, void*, GError**, ...);
using FreeFn = void (*)(char*);
using ErrorFreeFn = void (*)(GError*);

struct Lib {
    void* handle = nullptr;
    StoreFn store = nullptr;
    LookupFn lookup = nullptr;
    ClearFn clear = nullptr;
    FreeFn freePassword = nullptr;
    ErrorFreeFn freeError = nullptr;
    Lib() {
        handle = dlopen("libsecret-1.so.0", RTLD_LAZY | RTLD_LOCAL);
        if (handle == nullptr) return;
        store = reinterpret_cast<StoreFn>(dlsym(handle, "secret_password_store_sync"));
        lookup = reinterpret_cast<LookupFn>(dlsym(handle, "secret_password_lookup_sync"));
        clear = reinterpret_cast<ClearFn>(dlsym(handle, "secret_password_clear_sync"));
        freePassword = reinterpret_cast<FreeFn>(dlsym(handle, "secret_password_free"));
        freeError = reinterpret_cast<ErrorFreeFn>(dlsym(handle, "g_error_free"));
    }
    bool ok() const { return store && lookup && clear && freePassword && freeError; }
};
const Lib& lib() {
    static const Lib l;
    return l;
}
const SecretSchema* schema() {
    static const SecretSchema s = [] {
        SecretSchema x{};
        x.name = "org.lmw-synth.Assistant";
        x.flags = 0;                                               // SECRET_SCHEMA_NONE
        x.attributes[0] = {"app", 0};                              // SECRET_SCHEMA_ATTRIBUTE_STRING
        x.attributes[1] = {"account", 0};
        x.attributes[2] = {nullptr, 0};
        return x;
    }();
    return &s;
}
std::string take(GError* e, const char* what) {
    std::string t = std::string(what) + " failed" + (e != nullptr && e->message != nullptr ? std::string(": ") + e->message : std::string());
    if (e != nullptr) lib().freeError(e);
    return t;
}
const char* noKeychain = "no system keychain (libsecret) found: keep the key in the settings file instead";
} // namespace

bool available() { return lib().ok(); }

std::optional<std::string> load(std::string& error, const std::string& account) {
    error.clear();
    if (!lib().ok()) { error = noKeychain; return std::nullopt; }
    GError* e = nullptr;
    char* p = lib().lookup(schema(), nullptr, &e, "app", "LMW-Synth", "account", account.c_str(), static_cast<char*>(nullptr));
    if (e != nullptr) { error = take(e, "reading the key from the keychain"); return std::nullopt; }
    if (p == nullptr) return std::nullopt;
    std::string key(p);
    lib().freePassword(p);
    return key;
}

bool save(const std::string& key, std::string& error, const std::string& account) {
    if (!lib().ok()) { error = noKeychain; return false; }
    GError* e = nullptr;
    const int ok = lib().store(schema(), nullptr, "LMW-Synth assistant API key", key.c_str(), nullptr, &e,
                               "app", "LMW-Synth", "account", account.c_str(), static_cast<char*>(nullptr));
    if (e != nullptr || !ok) { error = take(e, "saving the key in the keychain"); return false; }
    return true;
}

bool clear(std::string& error, const std::string& account) {
    if (!lib().ok()) { error = noKeychain; return false; }
    GError* e = nullptr;
    lib().clear(schema(), nullptr, &e, "app", "LMW-Synth", "account", account.c_str(), static_cast<char*>(nullptr));
    if (e != nullptr) { error = take(e, "removing the key from the keychain"); return false; }
    return true;
}

#endif

} // namespace lmw::keystore
