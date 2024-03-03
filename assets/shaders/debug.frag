#version 460

in vec3 gColor;
in noperspective float dist;
layout(location = 0) out vec4 fragColor;

void main() {
    float a = exp2(-dist * dist);
    fragColor = vec4(gColor, a);
}