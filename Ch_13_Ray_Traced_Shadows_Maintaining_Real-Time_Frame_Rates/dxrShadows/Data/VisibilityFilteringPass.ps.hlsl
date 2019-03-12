__import ShaderCommon;
__import Shading;
#include "CommonUtils.hlsl"

#ifndef _LIGHT_COUNT_PASS_0
#define _LIGHT_COUNT_PASS_0 1 //< Default to one light
#endif

#ifndef _LIGHT_COUNT_PASS_1
#define _LIGHT_COUNT_PASS_1 1 //< Default to one light
#endif

#ifndef BLUR_PASS_DIRECTION
    #define BLUR_PASS_DIRECTION BLUR_HORIZONTAL
#endif

Texture2D depthBuffer;
Texture2D normalsBuffer;
Texture2D inputBuffer[MAX_LIGHTS_PER_PASS_QUARTER];
Texture2D variationBuffer[MAX_LIGHTS_PER_PASS_QUARTER];

cbuffer FilteringPassCB
{
	float depthRelativeDifferenceEpsilonMin;
	float depthRelativeDifferenceEpsilonMax;
	float dotNormalsEpsilon;
	float farOverNear;
    float maxVariationLevel;
};

struct PsOut
{
    float4 visibility0to3 : SV_TARGET0; //< Visibility buffers for 4 lights (0 to 3) packed into single texture
    float4 visibility4to7 : SV_TARGET1; //< Visibility buffers for 4 lights (4 to 7) packed into single texture
};

static const float gauss[45] =
{
	// 1 tap
    0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,

	// 3 taps
	0.0f, 0.0f, 0.0f, 0.196842f, 0.606316f, 0.196842f, 0.0f, 0.0f, 0.0f,

	// 5 taps
	0.0f, 0.0f, 0.05448868f, 0.2442013f, 0.40262f, 0.2442013f, 0.05448868f, 0.0f, 0.0f,

	// 7 taps
	0.0f, 0.02397741f, 0.09784279f, 0.2274913f, 0.301377f, 0.2274913f, 0.09784279f, 0.02397741f, 0.0f,

	// 9 taps
	0.01351957f, 0.04766219f, 0.1172301f, 0.2011676f, 0.2408413f, 0.2011676f, 0.11723f, 0.04766217f, 0.01351957f
};

inline bool isValidTap(float tapDepth, float centerDepth, float3 tapNormal, float3 centerNormal)
{

	// Adjust depth difference epsilon based on view space normal
	float dotViewNormal = abs(centerNormal.z);

	float depthRelativeDifferenceEpsilon = lerp(depthRelativeDifferenceEpsilonMax, depthRelativeDifferenceEpsilonMin, dotViewNormal);
		
	// Check depth
	if (abs(1.0f - (tapDepth / centerDepth)) > depthRelativeDifferenceEpsilon) return false;

	// Check normals
	if (dot(tapNormal, centerNormal) < dotNormalsEpsilon) return false;

	return true;
}


inline float4 blurPass(const int halfBlurSize, int4 kernelWeightOffsets, float4 kernelWeightBlends, int bufferIdx, int2 pos)
{

    int2 offset;
    float4 acc = 0.0f;
    float4 weight = 1.0f;
    int idx = 0;

    float centerDepth = getNormalizedLinearDepth(depthBuffer.Load(int3(pos, 0)).x, farOverNear);
	float3 centerNormal = normalize(normalsBuffer.Load(int3(pos.xy, 0)).xyz);

	[unroll]
    for (int i = -halfBlurSize; i <= halfBlurSize; ++i)
    {
        if (BLUR_PASS_DIRECTION == BLUR_HORIZONTAL)
			offset = pos + int2(i, 0);
        else
			offset = pos + int2(0, i);
			
		float tapDepth = getNormalizedLinearDepth(depthBuffer.Load(int3(offset, 0)).x, farOverNear);
		float3 tapNormal = normalize(normalsBuffer.Load(int3(offset, 0)).xyz);
        float4 tapShadow = inputBuffer[bufferIdx].Load(int3(offset, 0));

        float4 tapWeightLow = float4(gauss[kernelWeightOffsets.x + idx], gauss[kernelWeightOffsets.y + idx], gauss[kernelWeightOffsets.z + idx], gauss[kernelWeightOffsets.w + idx]);
        float4 tapWeightHigh = float4(gauss[kernelWeightOffsets.x + idx + 9], gauss[kernelWeightOffsets.y + idx + 9], gauss[kernelWeightOffsets.z + idx + 9], gauss[kernelWeightOffsets.w + idx + 9]);
        float4 tapWeight = lerp(tapWeightLow, tapWeightHigh, kernelWeightBlends);

        if (isValidTap(tapDepth, centerDepth, tapNormal, centerNormal))
        {
            acc += tapShadow * tapWeight;
        }
        else
        {
            weight -= tapWeight;
        }

        idx++;
    }

    return acc / weight;
}

inline float4 selectBlurPassSize(int bufferIdx, int2 pos) {

	float4 variation = variationBuffer[bufferIdx].Load(int3(pos.xy, 0));
	   
    float4 avgVariation = saturate(variation / maxVariationLevel);

    // Figure out filtering kernel size
    float4 kernelIdx = (avgVariation * (4.0f - 0.01f)); //< Subtract small number to prevent sampling out of gaussian kernel array for variation equal to 1
    int4 kernelWeightOffsets = int4(floor(kernelIdx)) * 9;
    float4 kernelWeightBlends = frac(kernelIdx);

    return blurPass(4, kernelWeightOffsets, kernelWeightBlends, bufferIdx, pos);
}


inline float4 visibilityFilteringPass(int lightBufferIndex, int2 pos) {

    #ifdef FILTERING_OFF
        return inputBuffer[lightBufferIndex].Load(int3(pos, 0));
    #else
        return selectBlurPassSize(lightBufferIndex, pos);
    #endif
}

PsOut main(float2 uvs : TEXCOORD, float4 pos : SV_POSITION)
{
    PsOut output;

	if (_LIGHT_COUNT_PASS_0 > 0) output.visibility0to3 = visibilityFilteringPass(0, int2(pos.xy));
	if (_LIGHT_COUNT_PASS_1 > 0) output.visibility4to7 = visibilityFilteringPass(1, int2(pos.xy));

	return output;
}
