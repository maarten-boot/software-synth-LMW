// SPDX-License-Identifier: AGPL-3.0-or-later
// Spec S12.6: the API key in the system keychain — Windows Credential Manager (generic credential
// "LMW-Synth/<account>"), macOS Keychain (generic password, service "LMW-Synth", account <account>),
// Linux Secret Service through libsecret, loaded at run time (no build dependency; not available without
// libsecret-1.so.0). Errors come back as text. Blocking calls: the keychain may ask the user to unlock it.
#pragma once
#include <optional>
#include <string>

namespace lmw::keystore {

bool available();                                                  // the platform has a keychain we can use
// `account` is "assistant" for the synth; tests use their own so they never touch a real key.
std::optional<std::string> load(std::string& error, const std::string& account = "assistant");   // nothing + no error: none stored
bool save(const std::string& key, std::string& error, const std::string& account = "assistant");
bool clear(std::string& error, const std::string& account = "assistant");                         // true also when none was stored

} // namespace lmw::keystore
