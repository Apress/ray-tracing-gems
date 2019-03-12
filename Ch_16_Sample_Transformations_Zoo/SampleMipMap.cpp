int2 SampleMipMap(Texture& T, float u[2], float *pdf)
{
    // Iterate over mipmaps of size 2x2 ... NxN.
    // load(x,y,mip) loads a texel (mip 0 is the largest power of two)
    int x = 0, y = 0;
    for (int mip = T.maxMip()-1; mip >= 0; --mip) {
        x <<= 1; y <<= 1;
        float left = T.load(x, y, mip) + T.load(x, y+1, mip);
        float right = T.load(x+1, y, mip) + T.load(x+1, y+1, mip);
        float probLeft = left / (left + right);
        if (u[0] < probLeft) {
            u[0] /= probLeft;
            float probLower = T.load(x, y, mip) / left;
            if (u[1] < probLower) {
                u[1] /= probLower;
            }
            else {
                y++;
                u[1] = (u[1] - probLower) / (1.0f - probLower);
            }
        }
        else {
            x++;
            u[0] = (u[0] - probLeft) / (1.0f - probLeft);
            float probLower = T.load(x, y, mip) / right;
            if (u[1] < probLower) {
                u[1] /= probLower;
            }
            else {
                y++;
                u[1] = (u[1] - probLower) / (1.0f - probLower);
            }
        }
    }
    // We have found a texel (x,y) with probability proportional to 
    // its normalized value. Compute the PDF and return the
    // coordinates.
    *pdf = T.load(x, y, 0) / T.load(0, 0, T.maxMip());
    return int2(x, y);
}
