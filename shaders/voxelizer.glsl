uint FindDominantAxis(vec3 n) {
    vec3 axisWeight = abs(n);
    uint dominantAxis = axisWeight.x > axisWeight.y ? 0 : 1;
    dominantAxis = dominantAxis > axisWeight.z ? dominantAxis : 2;
    return dominantAxis;
}

vec3 ProjectAlongDominantAxis(vec3 p, uint dominantAxis) {
    if (dominantAxis == 0)
        p = p.zyx;
    else if (dominantAxis == 1)
        p = p.xzy;
    p.z = 0;
    return p;
}