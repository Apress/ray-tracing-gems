// Kernel code describing variance tracking.

float quantile = 1.959964f; // 95% confidence interval
float stdError = sqrt(float(varianceOfMean / sampleCount));
bool hasConverged =
        (stdError * quantile) <= (convergenceErrorThres * mean);
