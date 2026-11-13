#pragma once
#include "dllmain.h"

namespace Hooks_Decryption {
    // LoadDepotDecryptionKey hook: serves user-provided decryption keys for
    // depots configured via Lua.
    void Install();
    void Uninstall();

    std::vector<uint8_t> GetCacheAppOwnershipTicket(AppId_t appId);

    // Dumps real AppTickets cached in Steam's in-memory ConfigStore
    // ("apptickets\<appId>") to <steamRoot>\opensteamtool\tickets\<appId>.bin.
    // The account must actually own the apps; tickets only exist after the
    // client fetched them from CM during login.
    void StartTicketDump(const char* steamRoot);
}
