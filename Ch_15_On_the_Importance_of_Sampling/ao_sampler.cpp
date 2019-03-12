float ao(float3 p, float3 n, int nSamples) {
   float a = 0;
   for (int i = 0; i < nSamples; ++i) {
     float xi[2] = { rng(), rng() };
     float3 dir(sqrt(xi[0]) * cos(2 * Pi * xi[1]),
                sqrt(xi[0]) * sin(2 * Pi * xi[1]),
                sqrt(1 - xi[0]));
     dir = transformToFrame(n, dir);
     if (visible(p, dir)) a += 1;
   }
   return a / nSamples;
}
