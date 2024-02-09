#version 450

vec2 positions[6] = vec2[](
   vec2(-1.0f, -1.0f),
   vec2( 1.0f,  1.0f),
   vec2(-1.0f,  1.0f),

   vec2( 1.0f,  1.0f),
   vec2(-1.0f, -1.0f),
   vec2( 1.0f, -1.0f)
);

layout(location = 0) out vec2 uv;

void main()
{   
   vec2 position = positions[gl_VertexID];
   gl_Position = vec4(position, 0.0f, 1.0f);

   uv = position * 0.5f + 0.5f;
}

