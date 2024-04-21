#version 460

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec2 vUV;

const float threshold = 0.05;
float getColor(vec2 uv) {
    vec2 dF = fwidth(uv);
    float edge = max(dF.x, dF.y);
    uv = 1.0 - abs(uv);
    uv = smoothstep(threshold + edge, threshold - edge, fract(uv));
    return (1.0 - uv.x) * (1.0 - uv.y);
}

void main() {
    vec3 n = normalize(vNormal);
    //vec3 col = vec3(0.0f);
    //col += max(dot(n, vec3(0.5f)), 0.0f);
    //col += 0.1f;
     vec3 col = (vNormal * 0.5 + 0.5f) * getColor(vUV);
    //  vec3 col = vec3(vNormal);
    fragColor = vec4(col, 1.0f);
}
