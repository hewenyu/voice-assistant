#pragma once

#include <string>

#ifdef _WIN32
#include <windows.h>
#include <rpc.h>
#pragma comment(lib, "rpcrt4.lib")
#else
#include <uuid/uuid.h>
#endif

namespace voice {

class UuidGenerator {
public:
    static std::string GenerateUuid() {
#ifdef _WIN32
        UUID uuid;
        UuidCreate(&uuid);
        unsigned char* str;
        UuidToStringA(&uuid, &str);
        std::string result(reinterpret_cast<char*>(str));
        RpcStringFreeA(&str);
        return result;
#else
        uuid_t uuid;
        char uuid_str[37];
        uuid_generate(uuid);
        uuid_unparse_lower(uuid, uuid_str);
        return std::string(uuid_str);
#endif
    }
};

} // namespace voice 