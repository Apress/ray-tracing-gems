// Host code for computing the sample ratio used by the performance budgeting system.

const float tracingBudgetInMs = 16.0f;
const float dampingFactor = 0.9f;                  // 90% (empirical)
const float stableArea = tracingBudgetInMs*0.15f;  // 15% of the budget

float sampleRatio = getLastFrameRatio();
float timeSpentTracing = getGPUTracingTime();
float boostFactor =
        clamp(0.25f, 1.0f, tracingBudgetInMs / timeSpentTracing);

if (abs(timeSpentTracing - tracingBudgetInMs) > stableArea)
    if (traceTime > tracingBudgetInMs)
        sampleRatio *= dampingFactor * boostFactor;
    else
        sampleRatio /= dampingFactor;

sampleRatio = clamp(0.001f, 1.0f, sampleRatio);
