#include "engine/Engine.h"

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hPrev, LPSTR lpCmdLine, int n) {
#ifdef _DEBUG
    AllocConsole();
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "r", stdin);
    std::cout.clear();
    std::clog.clear();
    std::cerr.clear();
    std::cin.clear();
    std::cout << "--- Debug console initialized ---" << std::endl;
#endif

    Engine engine;
    if (engine.OnInit(hi, n)) {
        engine.Run();
    }

#ifdef _DEBUG
    FreeConsole();
#endif
    return 0;
}
