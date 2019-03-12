dist = (rgb - rgb_mean) / rgb_deviation;
w = exp2(-10 * luma(dist));
