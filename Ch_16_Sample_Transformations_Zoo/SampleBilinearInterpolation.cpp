// First, sample in the v dimension. Compute the endpoints of
// the line that is the average of the two lines at the edges
// at u = 0 and u = 1.
float v0 = v[0] + v[1], v1 = v[2] + v[3];
// Sample along that line.
p[1] = SampleLinear(u[1], v0, v1);
// Now, sample in the u direction from the two line endpoints
// at the sampled v position.
p[0] = SampleLinear(u[0],
                    lerp(p[1], v[0], v[2]),
                    lerp(p[1], v[1], v[3]));
return p;
