#include "engine/EngineGlobals.h"
#include "engine/components/Input.h"

EngineConfig g_config;

int g_resizeWidth = 0;
int g_resizeHeight = 0;
bool g_resizeRequested = false;

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    ImGui_ImplWin32_WndProcHandler(h, m, w, l);
    Input::ProcessMessage(m, w, l);

    if (m == WM_SIZE) {
        if (w != SIZE_MINIMIZED) {
            g_resizeWidth = LOWORD(l);
            g_resizeHeight = HIWORD(l);
            g_resizeRequested = true;
        }
    }
    else if (m == WM_DESTROY) {
        PostQuitMessage(0);
    }

    return DefWindowProc(h, m, w, l);
}
