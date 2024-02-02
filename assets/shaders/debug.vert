#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;

uniform mat4 VP;

out vec3 vColor;

void main() {
   gl_Position = VP * vec4(position, 1.0f);
   vColor = color;
}