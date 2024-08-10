const float VOXEL_GRID_SIZE = 128;

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

// Writing to gl_Position outside the range results in
// clipping which is discarded by the rasterizer. Our goal
// is to maximize the number of triangle generated within
// the viewport
vec3 ToVoxelSpace(vec3 p) {
    return p / (VOXEL_GRID_SIZE * 0.5);
}

// p is in range -VOXEL_GRID_SIZE * 0.5, VOXEL_GRID_SIZE * 0.5
// return in range 0 to VOXEL_GRID_SIZE
vec3 WorldToTextureSpace(vec3 p) {
    return clamp(p + VOXEL_GRID_SIZE * 0.5, vec3(0.0f), vec3(VOXEL_GRID_SIZE));
}