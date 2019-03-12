if (u < 0.5) {
    u /= 0.5;
    return -r * SampleLinear(u, 1, 0);
} else {
    u = (u - .5) / .5;
    return r * SampleLinear(u, 1, 0);
}
