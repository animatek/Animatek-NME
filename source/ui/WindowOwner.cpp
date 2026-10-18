// No JUCE in this file: <windows.h> and JUCE's headers do not mix well in one translation unit.
#include "WindowOwner.h"

#if defined(_WIN32)
 #define WIN32_LEAN_AND_MEAN
 #define NOMINMAX
 #include <windows.h>

bool WindowOwner::setOwner (void* window, void* owner)
{
    if (window == nullptr || owner == nullptr)
        return false;
    // An owned window stays above its owner and goes with it when minimised.
    SetWindowLongPtrW (static_cast<HWND> (window), GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR> (owner));
    return true;
}

#elif defined(__linux__)
 #include <dlfcn.h>
 #include <cstdint>

// libX11 is opened at run time, as JUCE opens it, so NME does not link against it.
namespace
{
    using OpenDisplay  = void* (*) (const char*);
    using SetTransient = int (*) (void*, unsigned long, unsigned long);
    using Flush        = int (*) (void*);

    struct X11
    {
        void* display = nullptr;
        SetTransient setTransientForHint = nullptr;
        Flush flush = nullptr;

        X11()
        {
            void* lib = dlopen ("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
            if (lib == nullptr)
                return;
            auto open = reinterpret_cast<OpenDisplay> (dlsym (lib, "XOpenDisplay"));
            setTransientForHint = reinterpret_cast<SetTransient> (dlsym (lib, "XSetTransientForHint"));
            flush = reinterpret_cast<Flush> (dlsym (lib, "XFlush"));
            if (open != nullptr && setTransientForHint != nullptr && flush != nullptr)
                display = open (nullptr);
        }
    };

    X11& x11()
    {
        static X11 instance;
        return instance;
    }
}

bool WindowOwner::setOwner (void* window, void* owner)
{
    auto& x = x11();
    if (x.display == nullptr || window == nullptr || owner == nullptr)
        return false;
    // WM_TRANSIENT_FOR: the window manager keeps a transient window above the one it is for.
    x.setTransientForHint (x.display,
                           static_cast<unsigned long> (reinterpret_cast<std::uintptr_t> (window)),
                           static_cast<unsigned long> (reinterpret_cast<std::uintptr_t> (owner)));
    x.flush (x.display);
    return true;
}

#else

bool WindowOwner::setOwner (void*, void*) { return false; }

#endif
