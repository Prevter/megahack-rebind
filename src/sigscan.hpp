#pragma once
#include <psapi.h>

namespace sigscan {
    struct pattern_byte {
        enum type : uint8_t {
            exact,
            wildcard,
            cursor
        } type;
        uint8_t value;
    };

    inline bool match(const pattern_byte& pattern, uint8_t value) {
        return pattern.type == pattern_byte::wildcard || pattern.value == value;
    }

    inline size_t get_module_size(const char* module_name) {
        auto module = GetModuleHandle(module_name);
        if (!module) {
            return 0;
        }

        MODULEINFO info;
        GetModuleInformation(GetCurrentProcess(), module, &info, sizeof(info));
        return info.SizeOfImage;
    }

    inline std::vector<pattern_byte> parse_pattern(std::string_view pattern) {
        std::vector<pattern_byte> result;
        for (size_t i = 0; i < pattern.size(); i++) {
            // ignore whitespace
            if (pattern[i] == ' ') {
                continue;
            }

            if (pattern[i] == '?') {
                result.push_back({ pattern_byte::wildcard, 0 });
            } else if (pattern[i] == '^') {
                result.push_back({ pattern_byte::cursor, 0 });
            } else {
                auto value = std::stoi(pattern.substr(i, 2).data(), nullptr, 16);
                result.push_back({ pattern_byte::exact, static_cast<uint8_t>(value) });
                i++;
            }
        }
        return result;
    }

    inline void* scan(std::string_view pattern, uintptr_t base, size_t size) {
        auto pattern_bytes = parse_pattern(pattern);
        for (size_t i = 0; i < size; i++) {
            bool found = true;

            size_t current_offset = 0;
            size_t cursor_offset = i;

            for (const auto& pattern_byte : pattern_bytes) {
                auto ptr = reinterpret_cast<uint8_t*>(base + current_offset);
                if (cursor_offset >= size) {
                    found = false;
                    break;
                }

                if (pattern_byte.type == pattern_byte::cursor) {
                    cursor_offset = current_offset + i;
                    continue;
                }

                if (!match(pattern_byte, ptr[i])) {
                    found = false;
                    break;
                }

                current_offset++;
            }

            if (found) {
                return reinterpret_cast<void*>(base + cursor_offset);
            }
        }
        return nullptr;
    }

    inline void* scan(const char* pattern, const char* module_name) {
        auto base = GetModuleHandle(module_name);
        if (!base) {
            return nullptr;
        }

        auto size = get_module_size(module_name);
        return scan(pattern, reinterpret_cast<uintptr_t>(base), size);
    }

    inline uint8_t determine_operand_size(const uint8_t* instruction) {
        // example 1:
        // 48 89 3D 4A 4D 4B 00            mov     cs:qword_4B4D4A, rdi
        //         ^ operand starts here
        // result: 3

        switch (instruction[0]) {
            case 0x48:
                return 3;
            case 0x49:
                return 4;
            default:
                return 2;
        }
    }

    template <typename T>
    T static_lookup(std::string_view pattern, uintptr_t base, size_t size) {
        auto addr = scan(pattern, base, size);
        if (!addr) {
            return nullptr;
        }

        // determine which instruction to follow
        auto offset = determine_operand_size(static_cast<uint8_t*>(addr));
        auto relative = *reinterpret_cast<int32_t*>(reinterpret_cast<uintptr_t>(addr) + offset);

        // we now have the [rip+X] offset
        // now we need to add the current instruction pointer to it
        auto ripOffset = reinterpret_cast<uintptr_t>(addr) + offset + sizeof(int32_t);
        auto ptr = reinterpret_cast<T*>(ripOffset + relative);

        if (!ptr) {
            return nullptr;
        }

        return *ptr;
    }

    template <typename T>
    T static_lookup(std::string_view pattern, const char* module_name) {
        auto base = GetModuleHandle(module_name);
        if (!base) {
            return nullptr;
        }

        auto size = get_module_size(module_name);
        return static_lookup<T>(pattern, reinterpret_cast<uintptr_t>(base), size);
    }

}