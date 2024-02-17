#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

struct Key {
    bool isDown;
    bool wasDown;
};

struct Input {

    static Input *GetInstance() {
        static Input *input = new Input();
        return input;
    }

    void Initialize();

    Key *GetKey(int key);

    void SetKeyState(int key, bool state);

    void SetMousePos(const glm::vec2 &pos) {
        mouseDelta = pos - mousePos;
        mousePos = pos;
    }

    bool WasKeyPressed(int key) {
        Key *keyPtr = &keys[key];
        return keyPtr->isDown && !keyPtr->wasDown;
    }

    bool WasKeyReleased(int key) {
        Key *keyPtr = &keys[key];
        return keyPtr->wasDown && !keyPtr->isDown;
    }

    glm::vec2 GetMousePos() { return mousePos; }
    glm::vec2 GetMouseDelta() { return mouseDelta; }

    void Update();

    Key keys[512];

    glm::vec2 mousePos;
    glm::vec2 mouseDelta;
};