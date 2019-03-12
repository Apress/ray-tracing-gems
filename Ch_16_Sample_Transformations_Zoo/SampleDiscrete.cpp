int SampleDiscrete(std::vector<float> weights, float u,
            float *pdf, float *uRemapped) {
    float sum = std::accumulate(weights.begin(), weights.end(), 0.f);
    float uScaled = u * sum;
    int offset = 0;
    while (uScaled > weights[offset] && offset < weights.size()) {
        uScaled -= weights[offset];
        ++offset;
    }
    if (offset == weights.size()) offset = weights.size() - 1;

    *pdf = weights[offset] / sum;
    *uRemapped = uScaled / weights[offset];
    return offset;
}
