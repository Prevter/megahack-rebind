#pragma once
#include "sigscan.hpp"

class OctavusDirector {
public:
    using toggleUI_t = void(*)(OctavusDirector* self, int);
    inline static toggleUI_t toggleUI_orig = nullptr;
    inline static bool shouldPassToggleUI = false;

    static OctavusDirector* get() {
        // looking for
        // mov     cs:OctavusDirector::s_instance, rdi
        static auto instance = sigscan::static_lookup<OctavusDirector*>(
            "E8 ? ? ? ? 40 88 B7 ? ? ? ? ^ 48 89 3D",
            "absolllute.megahack.dll"
        );
        return instance;
    }

    void toggleUI(int idfkWhatThisParameterDoesButItSeemsToBeAlwaysZero = 0) {
        shouldPassToggleUI = true;
        toggleUI_orig(this, idfkWhatThisParameterDoesButItSeemsToBeAlwaysZero);
        shouldPassToggleUI = false;
    }
};