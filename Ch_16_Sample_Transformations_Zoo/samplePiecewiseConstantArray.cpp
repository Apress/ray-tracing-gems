vector<float> makePiecewiseConstantCDF(vector<float> pdf) {
    float total = 0.0;
    // CDF is one greater than PDF.
    vector<float> cdf { 0.0 };
    // Compute the cumulative sum.
    for (auto value : pdf) cdf.push_back(total += value);
    // Normalize.
    for (auto& value : cdf) value /= total;
    return cdf;
}

int samplePiecewiseConstantArray(float u, vector<float> cdf,
            float* uRemapped)
{
    // Use our (sorted) CDF to find the data point to the
    // left of our sample u.
    int offset = upper_bound(cdf.begin(), cdf.end(), u) -
    cdf.begin() - 1;
    *uRemapped = (u - cdf[offset]) / (cdf[offset+1] - cdf[offset]);
    return offset;
}
