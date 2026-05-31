#pragma once

#include "pch/Pch.h"

class Input {
private:
    static bool s_currentKeys[256];
    static bool s_previousKeys[256];

    static bool s_currentMouse[3]; // 0 = Left, 1 = Right, 2 = Middle
    static bool s_previousMouse[3];

    static SM::Vector2 s_mousePos;
    static SM::Vector2 s_mouseDelta;

public:
    static bool GetKey(int keyCode);
    static bool GetKeyDown(int keyCode);
    static bool GetKeyUp(int keyCode);
    static bool GetMouseButton(int button);
    static bool GetMouseButtonDown(int button);
    static bool GetMouseButtonUp(int button);
    static SM::Vector2 GetMousePosition();
    static SM::Vector2 GetMouseDelta();
    static void EndFrame();
    static void ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam);
};
