// SPDX-License-Identifier: AGPL-3.0-or-later
// lmw-mcp: the Model Context Protocol server of LMW-Synth (Spec S11). An AI assistant (Claude Code, Claude
// Desktop, Gemini CLI) starts it and talks JSON-RPC over stdin/stdout, one message per line; lmw-mcp
// changes the sound of a running LMW-Synth over 9P (Spec S10). Messages for people go to stderr only.
//   lmw-mcp [address]      address as for lmw9p; without one the newest running LMW-Synth is used
//   lmw-mcp --version
#include "GuideText.h"
#include "TutorialText.h"
#include "mcp/Server.h"
#include "ninep/Session.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <shlobj.h>
#else
#include <pwd.h>
#include <unistd.h>
#endif

namespace {

struct NinePLink final : lmw::mcp::Link {
    lmw::ninep::ClientSession s;
    std::vector<std::string> running() override { return lmw::ninep::runningSockets(); }
    bool connect(const std::string& a, std::string& e) override { return s.connect(a, e); }
    bool connected() const override { return s.connected(); }
    std::string address() const override { return s.address(); }
    std::optional<std::string> read(const std::string& p, std::string& e) override { return s.read(p, e); }
    bool write(const std::string& p, const std::string& t, std::string& e) override { return s.write(p, t, e); }
    void sleepMs(int ms) override { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
};

// The Documents folder as JUCE finds it (userDocumentsDirectory), where the synth keeps its presets.
std::string documentsDir() {
#ifdef _WIN32
    PWSTR path = nullptr;
    std::string out;
    if (SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &path) == S_OK) {
        const int n = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
        if (n > 1) { out.resize(static_cast<std::size_t>(n - 1)); WideCharToMultiByte(CP_UTF8, 0, path, -1, out.data(), n, nullptr, nullptr); }
    }
    CoTaskMemFree(path);
    return out;
#else
    std::string home;
    if (const char* h = std::getenv("HOME"); h != nullptr && *h) home = h;
    else if (const auto* pw = getpwuid(getuid()); pw != nullptr) home = pw->pw_dir;
#ifdef __APPLE__
    return home + "/Documents";
#else
    // XDG_DOCUMENTS_DIR from the user-dirs file, as JUCE reads it; else ~/Documents.
    std::string config;
    if (const char* x = std::getenv("XDG_CONFIG_HOME"); x != nullptr && *x) config = x;
    else config = home + "/.config";
    std::ifstream in(config + "/user-dirs.dirs");
    for (std::string l; std::getline(in, l);) {
        if (l.rfind("XDG_DOCUMENTS_DIR=", 0) != 0) continue;
        auto v = l.substr(18);
        if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
        if (v.rfind("$HOME", 0) == 0) v = home + v.substr(5);
        if (!v.empty()) return v;
    }
    return home + "/Documents";
#endif
#endif
}

template <std::size_t N> std::string joined(const char* const (&parts)[N]) {
    std::string s;
    for (const char* p : parts) s += p;
    return s;
}

} // namespace

int main(int argc, char** argv) {
    const std::string arg = argc > 1 ? argv[1] : "";
    if (arg == "--version") { std::printf("lmw-mcp %s\n", LMW_MCP_VERSION); return 0; }
    if (arg == "--help" || arg == "-h") {
        std::printf("usage: lmw-mcp [address]\nAn MCP server (stdio) for AI assistants; see README \"AI assistant\".\n");
        return 0;
    }
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    NinePLink link;
    lmw::mcp::Server server(link, {LMW_MCP_VERSION, documentsDir(), joined(lmw::guide::textParts),
                                   joined(lmw::tutorial::textParts)});
    if (!arg.empty()) {
        std::string e;
        if (!link.connect(arg, e)) std::fprintf(stderr, "lmw-mcp: %s (will try again when a tool is used)\n", e.c_str());
    }
    std::fprintf(stderr, "lmw-mcp %s: ready (MCP over stdio)\n", LMW_MCP_VERSION);
    for (std::string line; std::getline(std::cin, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.find_first_not_of(" \t") == std::string::npos) continue;
        if (const auto reply = server.handle(line)) {
            std::fwrite(reply->data(), 1, reply->size(), stdout);
            std::fputc('\n', stdout);
            std::fflush(stdout);
        }
    }
    return 0;
}
