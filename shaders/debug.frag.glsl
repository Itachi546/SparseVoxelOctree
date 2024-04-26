#version 460

layout(location = 0) in vec3 vColor;
layout(location = 0) out vec4 fragColor;

void main() {
    //float a = exp2(-dist * dist);
    fragColor = vec4(vColor, 1.0f);
}