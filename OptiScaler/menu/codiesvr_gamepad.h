#pragma once

#include <windows.h>
#include <xinput.h>
#include <imgui/imgui.h>
#include <algorithm>

// Poll under OptiInput::ScopedHookBypass; only the UI reads unblocked state.
template <typename Poll>
void CodiesVrFeedGamepad(ImGuiIO& io, Poll poll) {
    XINPUT_STATE state{};
    bool connected = false;
    for (DWORD slot = 0; slot < XUSER_MAX_COUNT; ++slot) {
        XINPUT_STATE candidate{};
        if (poll(slot, &candidate) == ERROR_SUCCESS) {
            state = candidate;
            connected = true;
            break;
        }
    }
    if (connected) io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    else io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
    const auto& pad = state.Gamepad;
    const auto button = [&](ImGuiKey key, WORD mask) {
        io.AddKeyEvent(key, (pad.wButtons & mask) != 0);
    };
    const auto analog = [&](ImGuiKey key, int value, int start, int end) {
        const float amount = std::clamp(static_cast<float>(value - start) /
                                           static_cast<float>(end - start), 0.0f, 1.0f);
        io.AddKeyAnalogEvent(key, amount > 0.1f, amount);
    };
    button(ImGuiKey_GamepadStart, XINPUT_GAMEPAD_START);
    button(ImGuiKey_GamepadBack, XINPUT_GAMEPAD_BACK);
    button(ImGuiKey_GamepadFaceDown, XINPUT_GAMEPAD_A);
    button(ImGuiKey_GamepadFaceRight, XINPUT_GAMEPAD_B);
    button(ImGuiKey_GamepadFaceLeft, XINPUT_GAMEPAD_X);
    button(ImGuiKey_GamepadFaceUp, XINPUT_GAMEPAD_Y);
    button(ImGuiKey_GamepadDpadLeft, XINPUT_GAMEPAD_DPAD_LEFT);
    button(ImGuiKey_GamepadDpadRight, XINPUT_GAMEPAD_DPAD_RIGHT);
    button(ImGuiKey_GamepadDpadUp, XINPUT_GAMEPAD_DPAD_UP);
    button(ImGuiKey_GamepadDpadDown, XINPUT_GAMEPAD_DPAD_DOWN);
    button(ImGuiKey_GamepadL1, XINPUT_GAMEPAD_LEFT_SHOULDER);
    button(ImGuiKey_GamepadR1, XINPUT_GAMEPAD_RIGHT_SHOULDER);
    button(ImGuiKey_GamepadL3, XINPUT_GAMEPAD_LEFT_THUMB);
    button(ImGuiKey_GamepadR3, XINPUT_GAMEPAD_RIGHT_THUMB);
    analog(ImGuiKey_GamepadL2, pad.bLeftTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD, 255);
    analog(ImGuiKey_GamepadR2, pad.bRightTrigger, XINPUT_GAMEPAD_TRIGGER_THRESHOLD, 255);
    analog(ImGuiKey_GamepadLStickLeft, pad.sThumbLX, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
    analog(ImGuiKey_GamepadLStickRight, pad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, 32767);
    analog(ImGuiKey_GamepadLStickUp, pad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, 32767);
    analog(ImGuiKey_GamepadLStickDown, pad.sThumbLY, -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, -32768);
    analog(ImGuiKey_GamepadRStickLeft, pad.sThumbRX, -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, -32768);
    analog(ImGuiKey_GamepadRStickRight, pad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, 32767);
    analog(ImGuiKey_GamepadRStickUp, pad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, 32767);
    analog(ImGuiKey_GamepadRStickDown, pad.sThumbRY, -XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, -32768);
}
