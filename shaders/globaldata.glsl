layout(binding = 0, set = 0) uniform GlobalData {
    mat4 uInvP;
    mat4 uInvV;

    mat4 P;
    mat4 V;
    mat4 VP;

    vec3 uLightPosition;
    float uScreenWidth;

    vec3 uCameraPosition;
    float uScreenHeight;
    float time;
};
