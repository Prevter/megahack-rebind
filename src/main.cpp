#include <Geode/Geode.hpp>
#include <geode.custom-keybinds/include/Keybinds.hpp>

#include <psapi.h>
#include <sinaps.hpp>

using namespace geode::prelude;
using namespace keybinds;

constexpr int HOTKEY_TOGGLE_UI = 0;
static bool g_passthroughPress = false;

class CoreDirector {
public:
    using GetInstanceFunc = CoreDirector* (*)();
    using OnHotkeyFunc = void(*)(CoreDirector*, int);

    static Result<void, std::string_view> initialize() {
        auto mhHandle = GetModuleHandleW(L"absolllute.megahack.dll");
        if (!mhHandle) {
            return Err("Mega Hack module not found");
        }

        MODULEINFO info;
        GetModuleInformation(GetCurrentProcess(), mhHandle, &info, sizeof(info));

        auto mhBase = reinterpret_cast<void*>(mhHandle);
        auto mhSize = info.SizeOfImage;

        auto getInstanceOffset = sinaps::find<"55 48 83 EC 30 48 8D 6C 24 30 48 C7 45 F8">(
            static_cast<uint8_t const*>(mhBase), mhSize);
        if (getInstanceOffset == sinaps::not_found) {
            return Err("Failed to find CoreDirector::get");
        }

        auto onHotkeyOffset = sinaps::find<"55 56 57 53 48 83 EC 68 48 8D 6C 24 60 48 C7 45 00 FE FF FF FF 89 D3 48 89 CE E8 ? ? ? ? 40 B7 01 84 C0">(
            static_cast<uint8_t const*>(mhBase), mhSize);
        if (onHotkeyOffset == sinaps::not_found) {
            return Err("Failed to find CoreDirector::onHotkey");
        }

        m_getInstance = reinterpret_cast<GetInstanceFunc>((void*)(static_cast<uint8_t const*>(mhBase) + getInstanceOffset));
        m_onHotkey = reinterpret_cast<OnHotkeyFunc>((void*)(static_cast<uint8_t const*>(mhBase) + onHotkeyOffset));

        return Ok();
    }

    static CoreDirector* get() {
        if (!m_getInstance) {
            log::error("GetInstance function not set!");
            return nullptr;
        }
        return m_getInstance();
    }

    void onHotkey(int hotkeyID) {
        if (!m_onHotkey) {
            log::error("OnHotkey function not set!");
            return;
        }

        m_onHotkey(this, hotkeyID);
    }

    static intptr_t getOnHotkeyAddress() {
        return reinterpret_cast<intptr_t>(m_onHotkey);
    }

private:
    inline static GetInstanceFunc m_getInstance = nullptr;
    inline static OnHotkeyFunc m_onHotkey = nullptr;
};

void onHotkey_hook(CoreDirector* self, int hotkeyID) {
    if (g_passthroughPress || hotkeyID != HOTKEY_TOGGLE_UI) {
        return self->onHotkey(hotkeyID);
    }
}

$on_mod(Loaded) {
    auto res = CoreDirector::initialize();
    if (!res) {
        log::error("Failed to initialize CoreDirector: {}", res.unwrapErr());
        return;
    }

    // Hook the onHotkey function
    auto onHotkeyAddr = CoreDirector::getOnHotkeyAddress();
    auto hookRes = Mod::get()->hook((void*)onHotkeyAddr, onHotkey_hook, "CoreDirector::onHotkey");
    if (!hookRes) {
        log::error("Failed to hook CoreDirector::onHotkey: {}", hookRes.unwrapErr());
        return;
    }

    // Register the keybind
    BindManager::get()->registerBindable({
        "toggle-mh"_spr,
        "Toggle UI",
        "Open/close Mega Hack.",
        { Keybind::create(KEY_Tab) },
        "Mega Hack"
    });

    new EventListener([=](InvokeBindEvent* event) {
        if (event->isDown()) {
            g_passthroughPress = true;
            CoreDirector::get()->onHotkey(HOTKEY_TOGGLE_UI);
            g_passthroughPress = false;
        }
        return ListenerResult::Propagate;
    }, InvokeBindFilter(nullptr, "toggle-mh"_spr));
}