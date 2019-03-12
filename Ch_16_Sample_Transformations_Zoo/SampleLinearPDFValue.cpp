if (x < 0 || x > 1) return 0;
return lerp(x, a, b) / ((a + b) / 2);
