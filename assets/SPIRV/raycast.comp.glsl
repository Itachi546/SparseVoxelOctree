#version 460

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(binding = 0, set = 0) uniform GlobalData {
    mat4 uInvP;
    mat4 uInvV;

    vec3 uAABBMin;
    float uScreenWidth;

    vec3 uAABBMax;
    float uScreenHeight;

    vec3 uLightPosition;
    float _padding;
};

layout(rgba8, binding = 0, set = 1) writeonly uniform image2D uOutputTexture;

void main() {
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    vec4 color = vec4(1.0f, 0.0f, 0.0f, 1.0f) * vec4(uLightPosition, 1.0f);
    imageStore(uOutputTexture, uv, color);
}