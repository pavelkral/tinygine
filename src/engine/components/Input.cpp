#include "engine/components/Input.h"
#include "engine/EngineDependencies.h"

bool Input::s_currentKeys[256] = {false};
bool Input::s_previousKeys[256] = {false};
bool Input::s_currentMouse[3] = {false};
bool Input::s_previousMouse[3] = {false};
SM::Vector2 Input::s_mousePos = {0, 0};
SM::Vector2 Input::s_mouseDelta = {0, 0};

bool Input::GetKey(int keyCode) { return s_currentKeys[keyCode]; }

bool Input::GetKeyDown(int keyCode) {
    return s_currentKeys[keyCode] && !s_previousKeys[keyCode];
}

bool Input::GetKeyUp(int keyCode) {
    return !s_currentKeys[keyCode] && s_previousKeys[keyCode];
}

bool Input::GetMouseButton(int button) { return s_currentMouse[button]; }

bool Input::GetMouseButtonDown(int button) {
    return s_currentMouse[button] && !s_previousMouse[button];
}

bool Input::GetMouseButtonUp(int button) {
    return !s_currentMouse[button] && s_previousMouse[button];
}

SM::Vector2 Input::GetMousePosition() { return s_mousePos; }

SM::Vector2 Input::GetMouseDelta() { return s_mouseDelta; }

void Input::EndFrame() {
    memcpy(s_previousKeys, s_currentKeys, sizeof(s_currentKeys));
    memcpy(s_previousMouse, s_currentMouse, sizeof(s_currentMouse));
    s_mouseDelta = {0.0f, 0.0f};
}

void Input::ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (wParam < 256)
            s_currentKeys[wParam] = true;
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (wParam < 256)
            s_currentKeys[wParam] = false;
        break;

    case WM_LBUTTONDOWN:
        s_currentMouse[0] = true;
        break;
    case WM_LBUTTONUP:
        s_currentMouse[0] = false;
        break;
    case WM_RBUTTONDOWN:
        s_currentMouse[1] = true;
        break;
    case WM_RBUTTONUP:
        s_currentMouse[1] = false;
        break;
    case WM_MBUTTONDOWN:
        s_currentMouse[2] = true;
        break;
    case WM_MBUTTONUP:
        s_currentMouse[2] = false;
        break;

    case WM_MOUSEMOVE:
        s_mousePos.x = static_cast<float>(LOWORD(lParam));
        s_mousePos.y = static_cast<float>(HIWORD(lParam));
        break;

    case WM_INPUT: {
        UINT size = 0;
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr,
                        &size, sizeof(RAWINPUTHEADER));
        if (size > 0) {
            std::vector<BYTE> rawdata(size);
            if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT,
                                rawdata.data(), &size,
                                sizeof(RAWINPUTHEADER)) == size) {
                RAWINPUT *raw = reinterpret_cast<RAWINPUT *>(rawdata.data());
                if (raw->header.dwType == RIM_TYPEMOUSE) {
                    s_mouseDelta.x += static_cast<float>(raw->data.mouse.lLastX);
                    s_mouseDelta.y += static_cast<float>(raw->data.mouse.lLastY);
                }
            }
        }
        break;
    }
    }
}
