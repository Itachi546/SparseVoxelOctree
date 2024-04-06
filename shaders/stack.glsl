const int MAX_LEVELS = 12;
struct StackData {
    vec3 position;
    uint firstSibling;
    int idx;
    float size;
    float tMax;
} stack[MAX_LEVELS];

int stackPtr = 0;
void stack_reset() { stackPtr = 0; }
void stack_push(StackData data) { stack[stackPtr++] = data; }

StackData stack_pop() { return stack[--stackPtr]; }
bool stack_empty() { return stackPtr == 0; }