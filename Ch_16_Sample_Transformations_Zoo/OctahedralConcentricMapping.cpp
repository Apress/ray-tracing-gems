// Compute radius r (branchless).
u = 2*u - 1;
d = 1 - (abs(u[0]) + abs(u[1]));
r = 1 - abs(d);

// Compute phi in the first quadrant (branchless, except for the
// division-by-zero test), using sign(u) to map the result to the
// correct quadrant below.
phi = (r == 0) ? 0 : (M_PI/4) * ((abs(u[1]) - abs(u[0])) / r + 1);
f = r * sqrt(2 - r*r);
x = f * sign(u[0]) * cos(phi);
y = f * sign(u[1]) * sin(phi);
z = sign(d) * (1 - r*r);
pdf = 1 / (4*M_PI);
