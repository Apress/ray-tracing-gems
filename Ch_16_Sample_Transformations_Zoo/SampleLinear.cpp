float SampleLinear( float a, float b ) {
    if (a == b) return u;
    return clamp((a - sqrt(lerp(u, a * a, b * b))) / (a - b), 0, 1);
}
