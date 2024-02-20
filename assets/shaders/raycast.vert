#version 450

vec2 positions[3] = vec2[](
    vec2(-1.0f, 3.0f),
    vec2(3.0f, -1.0f),
    vec2(-1.0f, -1.0f));

void main() {
    vec2 position = positions[gl_VertexID];
    gl_Position = vec4(position, 0.0f, 1.0f);
}
