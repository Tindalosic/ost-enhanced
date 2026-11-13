#include "Hooks_Decryption.h"
#include "HookMacros.h"
#include "dllmain.h"
#include <windows.h>
#include <string>
#include <thread>
#include <chrono>
#include <fstream>
#include <sstream>

namespace {

    void* g_pConfigStoreLocal = nullptr;

    HOOK_FUNC(ConfigStoreGetBinary, int32, void* pObject, EConfigStore eConfigStore, const char* KeyName, char* Key, uint32 KeySize) {
        if (eConfigStore == k_EConfigStoreUserLocal && pObject && !g_pConfigStoreLocal) {
            g_pConfigStoreLocal = pObject;
            LOG_DECRYPTIONKEY_DEBUG("ConfigStoreGetBinary: captured local ConfigStore at {}", g_pConfigStoreLocal);

        }
        std::string name(KeyName);
        LOG_DECRYPTIONKEY_DEBUG("ConfigStore::GetBinary called for pObject={}, eConfigStore={}, KeyName='{}'", 
                                    pObject, static_cast<uint32>(eConfigStore), name);
        // Expected shape: ".../<DepotId>\DecryptionKey"
        if (size_t last = name.find("\\DecryptionKey"); last != std::string::npos) {
            if (size_t start = name.find_last_of("\\", last - 1); start != std::string::npos) {
                AppId_t depotId = std::stoul(name.substr(start + 1, last - start - 1));
                if (const auto& key = LuaConfig::GetDecryptionKey(depotId); !key.empty()) {
                    if (KeySize >= key.size()) {
                        LOG_DECRYPTIONKEY_INFO("Providing decryption key for depot {}: {}", depotId,
                                               spdlog::to_hex(key.data(), key.data() + key.size()));
                        memcpy(Key, key.data(), key.size());
                        return static_cast<int32>(key.size());
                    }
                    LOG_DECRYPTIONKEY_WARN("Decryption key for depot {} is too large ({} bytes) for buffer ({} bytes)",
                                            depotId, key.size(), KeySize);
                }
            }
        }
        return oConfigStoreGetBinary(pObject, eConfigStore, KeyName, Key, KeySize);
    }

    std::vector<uint8_t> ReadConfigStoreLocalBinary(const std::string& keyName) {
        if (!g_pConfigStoreLocal || !oConfigStoreGetBinary) {
            LOG_DECRYPTIONKEY_WARN("GetConfigStoreLocalBinary: ConfigStoreGetBinary not ready, cannot get binary value");
            return {};
        }

        std::vector<uint8_t> value(1024);
        int32 result = oConfigStoreGetBinary(g_pConfigStoreLocal, k_EConfigStoreUserLocal,
                                             keyName.c_str(),
                                             reinterpret_cast<char*>(value.data()),
                                             static_cast<uint32>(value.size()));
        if (result <= 0) {
            LOG_DECRYPTIONKEY_DEBUG("GetConfigStoreLocalBinary: failed to read KeyName='{}'", keyName);
            return {};
        }

        value.resize(result);
        LOG_DECRYPTIONKEY_DEBUG("GetConfigStoreLocalBinary: got value for KeyName='{}' ({} bytes)",
                                keyName, value.size());
        return value;
    }
}


namespace Hooks_Decryption {
    namespace {
        std::string JoinPath(const std::string& a, const std::string& b) {
            if (a.empty()) return b;
            if (a.back() == '\\' || a.back() == '/') return a + b;
            return a + "\\" + b;
        }

        void DumpTicketFor(const std::string& root, const std::string& appId) {
            const auto ticket = GetCacheAppOwnershipTicket(static_cast<AppId_t>(std::stoul(appId)));
            if (ticket.empty()) {
                LOG_DECRYPTIONKEY_DEBUG("ticket dump: no cached ticket for {}", appId);
                return;
            }
            const std::string dir = JoinPath(JoinPath(root, "opensteamtool"), "tickets");
            CreateDirectoryA(JoinPath(root, "opensteamtool").c_str(), nullptr);
            CreateDirectoryA(dir.c_str(), nullptr);
            const std::string file = JoinPath(dir, appId + ".bin");
            std::ofstream out(file, std::ios::binary | std::ios::trunc);
            if (!out) {
                LOG_DECRYPTIONKEY_WARN("ticket dump: cannot open {}", file);
                return;
            }
            out.write(reinterpret_cast<const char*>(ticket.data()), static_cast<std::streamsize>(ticket.size()));
            out.close();
            LOG_DECRYPTIONKEY_INFO("ticket dump: wrote {} ({} bytes)", file, ticket.size());
        }
    }

    void StartTicketDump(const char* steamRoot) {
        if (!steamRoot || !*steamRoot) return;
        const std::string root(steamRoot);
        std::thread([root] {
            // ConfigStore pointer is only captured after Steam touches user
            // local storage, so poll instead of assuming an ordering.
            for (int i = 0; i < 240; ++i) {
                if (g_pConfigStoreLocal) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            if (!g_pConfigStoreLocal) {
                LOG_DECRYPTIONKEY_WARN("ticket dump: ConfigStore never captured, abort");
                return;
            }

            const std::string req = JoinPath(JoinPath(root, "opensteamtool"), "ticket_request.txt");
            std::ifstream in(req);
            if (!in) {
                LOG_DECRYPTIONKEY_DEBUG("ticket dump: no request file at {}", req);
                return;
            }

            int done = 0;
            std::string line;
            while (std::getline(in, line)) {
                while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t'))
                    line.pop_back();
                if (line.empty() || line[0] == '#') continue;
                // accept "appid" or "appid<whitespace>name"
                const auto sp = line.find_first_of(" \t");
                const std::string id = line.substr(0, sp);
                if (id.empty()) continue;
                bool digits = true;
                for (char c : id) if (c < '0' || c > '9') { digits = false; break; }
                if (!digits) continue;
                try { DumpTicketFor(root, id); ++done; } catch (...) {}
            }
            in.close();

            const std::string doneFile = JoinPath(JoinPath(root, "opensteamtool"), "ticket_dump.done");
            std::ofstream out(doneFile, std::ios::trunc);
            if (out) { out << done; out.close(); }
            LOG_DECRYPTIONKEY_INFO("ticket dump: finished, {} apps processed", done);
        }).detach();
    }
}

namespace Hooks_Decryption {
    void Install() {
        HOOK_BEGIN();
        INSTALL_HOOK_C(ConfigStoreGetBinary);
        HOOK_END();
    }

    void Uninstall() {
        UNHOOK_BEGIN();
        UNINSTALL_HOOK_C(ConfigStoreGetBinary);
        UNHOOK_END();
    }

    std::vector<uint8_t> GetCacheAppOwnershipTicket(AppId_t appId) {
        std::vector<uint8_t> ticket = ReadConfigStoreLocalBinary(std::format("apptickets\\{}", appId));
        if (ticket.empty()) {
            LOG_DECRYPTIONKEY_DEBUG("no cached ticket for AppId {}", appId);
            return ticket;
        }
        LOG_DECRYPTIONKEY_DEBUG("got cached ticket for AppId {} ({} bytes)", appId, ticket.size());
        return ticket;
    }
}
