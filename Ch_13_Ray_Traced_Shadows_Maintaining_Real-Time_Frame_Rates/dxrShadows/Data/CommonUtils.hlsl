
#include "shaderCommon.h"


// Recalculate view space position from pixel depth in linear <0; 1> range using inverse projection matrix
inline float3 getViewSpacePositionUsingDepth(float2 uv, float pixelDepth, float4 screenUnprojection, float4 screenUnprojection2)
{
	float4 screenPosition = float4(uv * 2.0f - 1.0f, 1.0f, 1.0f);
	float3 ray = (screenPosition.xyz * screenUnprojection.xyz + screenUnprojection2.xyz) * screenUnprojection2.w;
	return ray * pixelDepth;
}

// Returns linear depth normalized into <0; 1> range (0 is in camera position, 1 is on far plane)
inline float getNormalizedLinearDepth(float z, float farOverNear)
{
	return 1.0f / ((1.0f - farOverNear) * z + farOverNear);
}

// Variation calculations
float calculateFilteredVariation(float4 visibilityHistory)
{
	float visibilityMin = min(visibilityHistory.w, min(visibilityHistory.z, min(visibilityHistory.x, visibilityHistory.y)));
	float visibilityMax = max(visibilityHistory.w, max(visibilityHistory.z, max(visibilityHistory.x, visibilityHistory.y)));

	float variation = (visibilityMax - visibilityMin);

    return abs(variation);
}

float3 getViewSpacePosition(Texture2D depths, float2 uvs, float2 pos, float farOverNear, float4 screenUnprojection, float4 screenUnprojection2) {
	return getViewSpacePositionUsingDepth(uvs, getNormalizedLinearDepth(depths.Load(int3(pos, 0)).r, farOverNear), screenUnprojection, screenUnprojection2) * float3(1, -1, 1);
}

float3 calculateNormal(Texture2D depths, float2 uvs, float2 pos, float farOverNear, float4 screenUnprojection, float4 screenUnprojection2, float2 texelSizeRcp) {

	float3 centerPosition = getViewSpacePosition(depths, uvs, pos, farOverNear, screenUnprojection, screenUnprojection2);

	float3 upPosition = getViewSpacePosition(depths, uvs + float2(0, texelSizeRcp.y), pos + float2(0, 1), farOverNear, screenUnprojection, screenUnprojection2);
	float3 rightPosition = getViewSpacePosition(depths, uvs + float2(texelSizeRcp.x, 0), pos + float2(1, 0), farOverNear, screenUnprojection, screenUnprojection2);
	float3 downPosition = getViewSpacePosition(depths, uvs + float2(0, -texelSizeRcp.y), pos + float2(0, -1), farOverNear, screenUnprojection, screenUnprojection2);
	float3 leftPosition = getViewSpacePosition(depths, uvs + float2(-texelSizeRcp.x, 0), pos + float2(-1, 0), farOverNear, screenUnprojection, screenUnprojection2);

	float3 xSample1 = leftPosition - centerPosition;
	float3 xSample2 = centerPosition - rightPosition;
	float3 xSample = (length(xSample1) < length(xSample2)) ? xSample1 : xSample2;

	float3 ySample1 = upPosition - centerPosition;
	float3 ySample2 = centerPosition - downPosition;
	float3 ySample = (length(ySample1) < length(ySample2)) ? ySample1 : ySample2;

	return normalize(cross(xSample, ySample));
}

// Box filter over visibility in temporal domain
inline float calculateVisibilityFromHistory(float4 history)
{
    return dot(history, float4(0.25f));
}

static const int lightSamplesIndices[33] =
{
    0,
    0, 
    36, 
    36 + 72, 
    36 + 72 + 108, 
    36 + 72 + 108 + 144, 
    36 + 72 + 108 + 144 + 180,
    36 + 72 + 108 + 144 + 180 + 216,
    36 + 72 + 108 + 144 + 180 + 216 + 252,

    // 9-12
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288,

    // 13-16
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432,

    // 17-20
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576,

    // 21-24
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576 + 720,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576 + 720,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576 + 720,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576 + 720,

    // 24-28
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576 + 720 + 864,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576 + 720 + 864,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576 + 720 + 864,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576 + 720 + 864,

    // 29-32
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576 + 720 + 864 + 1008,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576 + 720 + 864 + 1008,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576 + 720 + 864 + 1008,
    36 + 72 + 108 + 144 + 180 + 216 + 252 + 288 + 432 + 576 + 720 + 864 + 1008,
};

inline void pack4ToTexture(int i, float value, out float4 packedBuffer0To3, out float4 packedBuffer4To7)
{
	if (i == 0) packedBuffer0To3.r = value;
	else if (i == 1) packedBuffer0To3.g = value;
	else if (i == 2) packedBuffer0To3.b = value;
	else if (i == 3) packedBuffer0To3.a = value;
	else if (i == 4) packedBuffer4To7.r = value;
	else if (i == 5) packedBuffer4To7.g = value;
	else if (i == 6) packedBuffer4To7.b = value;
	else if (i == 7) packedBuffer4To7.a = value;
}

inline float unpackFrom4Texture(float4 packedBuffer0To3, float4 packedBuffer4To7, int lightIndex)
{
	if (lightIndex == 0) return packedBuffer0To3.x;
	else if (lightIndex == 1) return packedBuffer0To3.y;
	else if (lightIndex == 2) return packedBuffer0To3.z;
	else if (lightIndex == 3) return packedBuffer0To3.w;
	else if (lightIndex == 4) return packedBuffer4To7.x;
	else if (lightIndex == 5) return packedBuffer4To7.y;
	else if (lightIndex == 6) return packedBuffer4To7.z;
	else if (lightIndex == 7) return packedBuffer4To7.w;

	return 1.0f;
}

float4 encodeVariationAndSampleCount(float4 newHistory, int4 samplesCounts)
{
    return (newHistory * 0.99f) + float4(samplesCounts) + float4(1.0f); //< Add 1 to encode "0 samples" correctly
}

float4 decodeVariationAndSampleCount(float4 history, out int4 samplesCounts)
{
	samplesCounts = int4(trunc(history)) - int4(1);
	return frac(history) / 0.99f;
}
