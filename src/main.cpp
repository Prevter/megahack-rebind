#include <Geode/Geode.hpp>
#include <geode.custom-keybinds/include/Keybinds.hpp>

#include "octavus.hpp"
#include "sigscan.hpp"

using namespace geode::prelude;
using namespace keybinds;

void toggleUI_hook(OctavusDirector* self, int p0) {
    if (OctavusDirector::shouldPassToggleUI) {
        OctavusDirector::toggleUI_orig(self, p0);
    }
}

$on_mod(Loaded) {
    // Find addresses and verify
    auto toggleUI_addr = sigscan::scan("CC ^ ? ? ? ? ? ? ? ? ? ? ? ? ? ? 50 48 8B 05 ? ? ? ? 48 33 C4 48 89 44 24 40 8B DA", "absolllute.megahack.dll");
    if (!toggleUI_addr) {
        log::error("Failed to find OctavusDirector::toggleUI");
        return;
    }

    if (!OctavusDirector::get()) {
        log::error("Failed to find OctavusDirector instance");
        return;
    }

    // Hook the function
    OctavusDirector::toggleUI_orig = reinterpret_cast<OctavusDirector::toggleUI_t>(toggleUI_addr);

    auto res = Mod::get()->hook(toggleUI_addr, toggleUI_hook, "OctavusDirector::toggleUI");
    if (!res) {
        log::error("Failed to hook OctavusDirector::toggleUI");
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
            OctavusDirector::get()->toggleUI(0);
        }
        return ListenerResult::Propagate;
    }, InvokeBindFilter(nullptr, "toggle-mh"_spr));
}
