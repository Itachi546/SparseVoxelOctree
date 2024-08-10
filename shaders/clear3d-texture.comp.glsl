#version 460

layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(r8, binding = 0, set = 0) uniform image3D uTexture;

void main() {
    ivec3 coord = ivec3(gl_GlobalInvocationID.xyz);
    imageStore(uTexture, coord, vec4(0.0f));
}