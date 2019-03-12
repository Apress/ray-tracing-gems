__import ShaderCommon;
__import Shading;

#include "CommonUtils.hlsl"

cbuffer DebugPassCB
{
	float4x4 invView;
	float4x4 transposeView;	
    float4 screenUnprojection;
    float4 screenUnprojection2;
    float farOverNear;
    float aspectRatio;
	float maxVelocity;
	float runningAverageAlphaMin;
    uint samplesCount;
    int maxRotations;
	float2 viewportDims;
	int visibilityBufferToDisplay;
    int frameToVisualize;
    int restrictSamplesCount;
    int samplesOffset;
	uint cullingBlockSize;
	float maxVariationLevel;
	uint frameNumber;
};

#ifndef _LIGHT_COUNT
	#define _LIGHT_COUNT 1 //< Default to one light
#endif

Texture2D motionAndDepthBuffer;
Texture2D variationBuffer[MAX_LIGHTS_PER_PASS_QUARTER];
Texture2D variationHistoryBuffer[MAX_LIGHTS_PER_PASS];
Texture2D depthBuffer;
Texture2D normalsBuffer;
Texture1D samplesBuffer;

float distanceFromLineSegment(float2 p, float2 start, float2 end) {
	
	float l2 = pow(length(start - end), 2); 
	if (l2 == 0.0f) return length(p - start);   
	float t = max(0.0f, min(1.0f, dot(p - start, end - start) / l2));
	float2 projection = start + t * (end - start);
	return length(p - projection);
}

float3 visualizeMotionVectors(float2 pos)
{
	float lineThickness = 2.0f;
	float blockSize = 64.0f;

	// Get pixel position for which to calculate vector (in the center of the blockSize x blockSize pixels neighbourhood)
	float2 pixelPos = int2(pos / blockSize) * blockSize + (blockSize * 0.5f);

	// Get motion vector end point
    float2 previousPos01 = motionAndDepthBuffer.Load(int3(pixelPos, 0)).xy;
    float2 previousPos = previousPos01 * viewportDims;

	// Reject motion vector for failed reprojection
    bool rejectReprojection = (previousPos01.x == INVALID_UVS.x && previousPos01.y == INVALID_UVS.y);
    if (rejectReprojection) previousPos = pixelPos;

	// Get distance from motion vector
	float lineDistance = distanceFromLineSegment(pos, pixelPos, previousPos);

	// Draw line based on distance
	return (lineDistance < lineThickness) ? float3(1.0f, 1.0f, 1.0f) : float3(0.0f, 0.0f, 0.0f);
}

inline float getVariationLevelValue(float4 packedVariationBuffer0To3, float4 packedVariationBuffer4To7, uint lightIndex) {

	if (lightIndex == 0) return packedVariationBuffer0To3.x;
	else if (lightIndex == 1) return packedVariationBuffer0To3.y;
	else if (lightIndex == 2) return packedVariationBuffer0To3.z;
	else if (lightIndex == 3) return packedVariationBuffer0To3.w;
	else if (lightIndex == 4) return packedVariationBuffer4To7.x;
	else if (lightIndex == 5) return packedVariationBuffer4To7.y;
	else if (lightIndex == 6) return packedVariationBuffer4To7.z;
	else if (lightIndex == 7) return packedVariationBuffer4To7.w;

	return 1.0f;
}


float4 main(float2 uvs : TEXCOORD, float4 pos : SV_POSITION) : SV_TARGET
{
	float linearDepth01 = getNormalizedLinearDepth(depthBuffer.Load(int3(pos.xy, 0)).r, farOverNear);
	float3 pixelViewSpacePosition = getViewSpacePositionUsingDepth(uvs, linearDepth01, screenUnprojection, screenUnprojection2) * float3(1, -1, 1);
	float4 pixelWorldSpacePosition = mul(float4(pixelViewSpacePosition, 1.0f), invView);
    pixelWorldSpacePosition /= pixelWorldSpacePosition.w;

	#ifdef OUTPUT_DEPTH
		return linearDepth01;
	#endif

	#ifdef OUTPUT_VIEW_SPACE_POSITION
		return float4(pixelViewSpacePosition, 1.0f);
	#endif

	#ifdef OUTPUT_WORLD_SPACE_POSITION
		return float4(pixelWorldSpacePosition.xyz, 1.0f);
	#endif

	#ifdef OUTPUT_MOTION_VECTORS
		return linearDepth01 + float4(visualizeMotionVectors(pos.xy), 0.0f);
	#endif

	#ifdef OUTPUT_VARIATION_LEVEL

        float4 packedVariationBuffer0To3 = variationBuffer[0].Load(int3(pos.xy, 0)).rgba;
        float4 packedVariationBuffer4To7 = variationBuffer[1].Load(int3(pos.xy, 0)).rgba;

        float variation = 0.0f;
        if (visibilityBufferToDisplay != INVALID_LIGHT_INDEX)
        {
            variation = getVariationLevelValue(packedVariationBuffer0To3, packedVariationBuffer4To7, visibilityBufferToDisplay);
        }
        else
        {

            [unroll]
            for (uint l = 0; l < _LIGHT_COUNT; l++)
            {
                float lightVariation = getVariationLevelValue(packedVariationBuffer0To3, packedVariationBuffer4To7, l);
                variation = max(variation, lightVariation);
            }

        }

        return float4(lerp(float3(1.0f, 1.0f, 0.5f), float3(1.0f, 0.0f, 1.0f), variation), 1.0f);
	
    #endif

    #ifdef OUTPUT_NORMALS_WORLD
		float3 viewSpaceNormal = normalize(normalsBuffer.Load(int3(pos.xy, 0)).xyz);
		float3 worldSpaceNormal = mul(float4(viewSpaceNormal, 0.0f), transposeView).rgb;
		return float4(worldSpaceNormal, 0.0f);
    #endif

    #ifdef OUTPUT_ADAPTIVE_SAMPLES_COUNT
		
		float4 result = 0.0f;
		int lightCount = _LIGHT_COUNT;
		int lightIndexStart = 0;

		if (visibilityBufferToDisplay != INVALID_LIGHT_INDEX) {
			lightIndexStart = visibilityBufferToDisplay;
			lightCount = 1;
		}

		for (int lightIndex = lightIndexStart; lightIndex < lightIndexStart + lightCount; lightIndex++) {

			int4 pastSampleCounts;
			float4 variationHistory = decodeVariationAndSampleCount(variationHistoryBuffer[lightIndex].Load(int3(pos.xy, 0)), pastSampleCounts);

			float4 minColor;

            minColor = lerp(float4(1.0f, 1.0f, 1.0f, 0.0f), float4(1.0f, 0.0f, 0.0f, 0.0f), float(pastSampleCounts.r) / 8.0f);

            if (pastSampleCounts.r == 0) minColor = float4(1.0f);

			result += minColor / float(lightCount);
		}

		return result;

#endif

#ifdef OUTPUT_FILTERING_KERNEL_SIZE

		float4 result = 0.0f;
		int lightCount = _LIGHT_COUNT;
		int lightIndexStart = 0;

		if (visibilityBufferToDisplay != INVALID_LIGHT_INDEX) {
			lightIndexStart = visibilityBufferToDisplay;
			lightCount = 1;
		}

		float4 inputVariation0To3 = variationBuffer[0].Load(int3(pos.xy, 0));
		float4 inputVariation4To7 = variationBuffer[1].Load(int3(pos.xy, 0));

		for (int lightIndex = lightIndexStart; lightIndex < lightIndexStart + lightCount; lightIndex++) {

			float variation = unpackFrom4Texture(inputVariation0To3, inputVariation4To7, lightIndex);

            float avgVariation = saturate(variation / maxVariationLevel);

			// Figure out filtering kernel size
			float kernelIdx = (avgVariation * (4.0f - 0.01f)); //< Subtract small number to prevent sampling out of gaussian kernel array for variation equal to 1
			int kernelWeightOffsets = int(floor(kernelIdx)) * 9;
			float kernelWeightBlends = frac(kernelIdx);

			float4 minColor;
			float4 maxColor;
			if (avgVariation < 0.25f) {
				minColor = float4(1.0f);
				maxColor = float4(0.0f, 0.0f, 1.0f, 0.0f);
			} else if (avgVariation < 0.5f) {
				minColor = float4(1.0f, 1.0f, 0.6f, 0.0f);
				maxColor = float4(1.0f, 1.0f, 0.0f, 0.0f);
			}
			else if (avgVariation < 0.75f) {
				minColor = float4(1.0f, 0.0f, 0.6f, 0.0f);
				maxColor = float4(1.0f, 0.0f, 0.0f, 0.0f);
			}
			else {
				minColor = float4(0.8f, 0.35f, 0.5f, 0.0f);
				maxColor = float4(0.7f, 0.2f, 0.35f, 0.0f);
			}
			
			result += (lerp(minColor, maxColor, kernelWeightBlends)) / float(lightCount);
		}

		return result;
#endif

    return float4(1.0f, 0.0f, 1.0f, 0.0f);
}
