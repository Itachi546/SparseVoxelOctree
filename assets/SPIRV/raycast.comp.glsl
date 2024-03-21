#version 460

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(rgba8, binding = 0) writeonly uniform image2D uOutputTexture;

void main() {
    ivec2 uv = ivec2(gl_GlobalInvocationID.xy);
    vec4 color = vec4(1.0f, 0.0f, 0.0f, 1.0f);
    imageStore(uOutputTexture, uv, color);
}