#version 460

const float LINE_WIDTH = 5.0f;
uniform vec2 uInvResolution;

layout(lines) in;
layout(triangle_strip, max_vertices = 4) out;

in vec3 vColor[];
out vec3 gColor;
out noperspective float dist;

void main() {
    vec4 p0 = gl_in[0].gl_Position;
    vec4 p1 = gl_in[1].gl_Position;

    if (p0.w > p1.w) {
        vec4 temp = p0;
        p0 = p1;
        p1 = temp;
    }
    const float nearPlane = 0.5f;
    if (p0.w < nearPlane) // section 1
    {
        float ratio = (nearPlane - p0.w) / (p1.w - p0.w);
        p0 = mix(p0, p1, ratio);
    }

    vec2 sp0 = p0.xy / p0.w;
    vec2 sp1 = p1.xy / p1.w;
    // Calculate offset along normal in screen space
    vec2 offset = normalize(vec2(sp0.y - sp1.y, sp1.x - sp0.x)) * uInvResolution * LINE_WIDTH;

    // Calculate the quad
    gl_Position = vec4(p0.xy + offset * p0.w, p0.zw);
    gColor = vColor[0];
    dist = LINE_WIDTH * 0.5f;
    EmitVertex();

    gl_Position = vec4(p0.xy - offset * p0.w, p0.zw);
    gColor = vColor[0];
    dist = -LINE_WIDTH * 0.5f;
    EmitVertex();

    gl_Position = vec4(p1.xy + offset * p1.w, p1.zw);
    dist = LINE_WIDTH * 0.5f;
    gColor = vColor[1];
    EmitVertex();

    gl_Position = vec4(p1.xy - offset * p1.w, p1.zw);
    dist = -LINE_WIDTH * 0.5f;
    gColor = vColor[1];
    EmitVertex();
}
