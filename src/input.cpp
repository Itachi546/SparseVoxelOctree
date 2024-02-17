#include "input.h"

void Input::Initialize() {
}

Key *Input::GetKey(int key) {
    return &keys[key];
}

void Input::SetKeyState(int key, bool state) {
    Key *keyPtr = &keys[key];
    keyPtr->isDown = state;
}

void Input::Update() {
    for (auto &key : keys) {
        key.wasDown = key.isDown;
    }

    mouseDelta.x = 0.0f;
    mouseDelta.y = 0.0f;
}