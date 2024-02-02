#version 460

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;

uniform mat4 uVP;

struct InstanceData {
   float x, y, z, w;
};

layout(std430, binding = 0) buffer DrawData {
   InstanceData instanceData[];
};

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec2 vUV;

void main() 
{
   InstanceData data = instanceData[gl_InstanceID];

   vec3 worldPos = position * data.w + vec3(data.x, data.y, data.z);

   vNormal = normal;
   vUV = position.xy * normal.z + position.yz * normal.x + position.xz * normal.y;

   gl_Position = uVP * vec4(worldPos, 1.0f);
}
