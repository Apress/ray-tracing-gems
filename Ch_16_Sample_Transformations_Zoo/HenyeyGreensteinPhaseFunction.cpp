phi = 2.0 * M_PI * u[0];
if (g != 0) {
    tmp = (1 - g * g) / (1 + g * (1 - 2 * u[1]));
    cos_theta = (1 + g * g - tmp * tmp) / (2 * g);
} else {
    cos_theta = 1 - 2 * u[1];
}
