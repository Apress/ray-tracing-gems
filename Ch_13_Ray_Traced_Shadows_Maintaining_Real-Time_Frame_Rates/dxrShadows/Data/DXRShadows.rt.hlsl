__import Raytracing;

#include "CommonUtils.hlsl"

// RT Shader inputs
Texture2D inputVisibilityCache[MAX_LIGHTS_PER_PASS];
Texture2D inputFilteredVariationBuffer[MAX_LIGHTS_PER_PASS_QUARTER];
Texture2D motionAndDepthBuffer;
Texture1D samples;

// Texture sampler
SamplerState bilinearSampler;

// RT Shader outputs
RWTexture2D<float4> outputVisibilityCache[MAX_LIGHTS_PER_PASS] : register(u0);
RWTexture2D<float4> outputVariationAndSampleCountCache[MAX_LIGHTS_PER_PASS] : register(u8); //< Keep register index in sync with MAX_LIGHTS_PER_PASS
RWTexture2D<float4> outputFilteredVisibilityBuffer[MAX_LIGHTS_PER_PASS_QUARTER] : register(u16); //< Keep register index in sync with MAX_LIGHTS_PER_PASS
RWTexture2D<float4> outputFilteredVariationBuffer[MAX_LIGHTS_PER_PASS_QUARTER] : register(u18); //< Keep register index in sync with MAX_LIGHTS_PER_PASS

// Parameters and defines

#ifndef _LIGHT_COUNT
#define _LIGHT_COUNT 1 //< Default to one light if not specified
#endif

shared cbuffer PerFrameCB
{

    // Projection parameters
	float4x4 invView;
	float4x4 view;
	float2 viewportDims;
	float2 texelSizeRcp;
	float tanHalfFovY;

	// Parameters for inverse projection 
	float4 screenUnprojection;
	float4 screenUnprojection2;

    // Quality settings
	int lightSamplesCount;
	int maxLightSamplesCount;
    float variationThreshold;

    // Global sample count limiting parameters
	int variationBufferBottomMipLevel;
	float inputFilteredVariationGlobalFalloffSpeed;
	float inputFilteredVariationGlobalThreshold;
	
	float maxRayLength;
	uint cullingBlockSize;

	uint frameNumber;
	int maxRayRecursionDepth;

	ShadowsLightInfo lightsInfo[MAX_LIGHTS_PER_PASS];
};

struct PrimaryRayData
{
	float4 color;
	int depth;
	float hitT;
};

struct ShadowRayData
{
	bool hit;
};

// ---------------------------------------------------------------------------------------
//    Utils
// ---------------------------------------------------------------------------------------

inline bool isPointLight(int lightIndex)
{
    return lightsInfo[lightIndex].type == POINT_LIGHT;
}

inline bool isSphericalLight(int lightIndex)
{
    return lightsInfo[lightIndex].type == SPHERICAL_LIGHT;
}

inline bool isHardDirectionalLight(int lightIndex)
{
    return lightsInfo[lightIndex].type == DIRECTIONAL_HARD_LIGHT;
}

inline bool isSoftDirectionalLight(int lightIndex)
{
    return lightsInfo[lightIndex].type == DIRECTIONAL_SOFT_LIGHT;
}

// ---------------------------------------------------------------------------------------
//    Shaders
// ---------------------------------------------------------------------------------------

[shader("miss")]
void shadowMiss(inout ShadowRayData hitData)
{
	hitData.hit = false;
}

[shader("anyhit")]
void shadowAnyHit(inout ShadowRayData hitData, in BuiltInTriangleIntersectionAttributes attribs)
{
	hitData.hit = true;
}

[shader("miss")]
void primaryMiss(inout PrimaryRayData hitData)
{
	hitData.hitT = -1;
}

[shader("closesthit")]
void primaryClosestHit(inout PrimaryRayData hitData, in BuiltInTriangleIntersectionAttributes attribs)
{
	// Get the hit-point data
	float3 rayOrigW = WorldRayOrigin();
	float3 rayDirW = WorldRayDirection();
	float hitT = RayTCurrent();
	uint triangleIndex = PrimitiveIndex();
	hitData.hitT = hitT;

	float3 posW = rayOrigW + hitT * rayDirW;

	VertexOut v = getVertexAttributes(triangleIndex, attribs);
	ShadingData sd = prepareShadingData(v, gMaterial, rayOrigW, 0);

	float4 matColor = float4(sd.diffuse, 1.0f - sd.opacity);
	hitData.color *= matColor;

	if (hitData.color.a > 0.0f && hitData.depth < maxRayRecursionDepth)
	{
		RayDesc ray;
		ray.Origin = posW;
		ray.Direction = rayDirW;
		ray.TMin = 0.001;
		ray.TMax = 100000;

		hitData.depth++;
		TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0, hitProgramCount, 0, ray, hitData);
	}

}

float checkLightHit(uint lightIndex, float3 origin, float3 direction, float rayLength)
{
	RayDesc ray;
	ray.Origin = origin;
	ray.Direction = normalize(direction);
	ray.TMin = 0.001;
	ray.TMax = max(0.01, rayLength);

	ShadowRayData rayData;
	rayData.hit = true;
	TraceRay(gRtScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 1, hitProgramCount, 1, ray, rayData);
	return rayData.hit ? 0.0f : 1.0f;
}

float sampleDiskLight(uint lightIndex, float3 lightCenterWorldSpacePosition, float lightWorldSpaceRadius, float3 viewSpacePixelPosition, float3 origin, uint2 launchIndex, bool isDirectional, const int samplesCountToUse, const int frameNumberToUse)
{
	float result = 1.0f;

	// Construct TBN matrix to position disk samples towards shadow ray direction
	float3 n = normalize(lightCenterWorldSpacePosition - origin);
	float3 rvec = normalize(viewSpacePixelPosition);
	float3 b1 = normalize(rvec - n * dot(rvec, n));
	float3 b2 = cross(n, b1);

	float3x3 tbn = float3x3(b1, b2, n);

	// Pick subset of samples to use based on "frame number % 4" and position on screen within a block of 3x3 pixels
	int pixelIdx = dot(int2(fmod(float2(launchIndex), 3)), int2(1, 3));
    int samplesStartIndex = lightSamplesIndices[samplesCountToUse] + (pixelIdx * samplesCountToUse) + int(frameNumberToUse * (samplesCountToUse * 9));

	float sampleWeight = 1.0f / float(samplesCountToUse);

	// Prepare ray data for each light sample
	RayDesc rayData;

	[unroll]
	for (int i = 0; i < samplesCountToUse; i++)
	{
		float2 diskSample = samples.Load(int2(samplesStartIndex + i, 0)).xy * lightWorldSpaceRadius;

        float sampleDistance;

        float3 sampleDirection = lightCenterWorldSpacePosition + (mul(float3(diskSample.x, diskSample.y, 0.0f), tbn)) - origin;
        float len = length(sampleDirection);

        if (isDirectional)
            sampleDistance = max(0.01, maxRayLength);
        else
            sampleDistance = max(0.01, len);
		
		rayData.Origin = origin;
		rayData.Direction = normalize(sampleDirection);
		rayData.TMin = 0.001;
        rayData.TMax = sampleDistance - rayData.TMin;

        ShadowRayData rayResult;
        rayResult.hit = true;

        TraceRay(gRtScene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH, 0xFF, 1, hitProgramCount, 1, rayData, rayResult);

        if (rayResult.hit)
            result -= sampleWeight;

	}

	return result;

}

inline float sampleLight(int lightIndex, float3 worldSpacePixelPosition, float3 viewSpacePixelPosition, uint2 launchIndex, int samplesCountToUse, int frameNumberToUse)
{
	if (isPointLight(lightIndex)) {

		// Point light
		float3 direction = lightsInfo[lightIndex].position - worldSpacePixelPosition;
		float rayLength = length(direction);

		return checkLightHit(lightIndex, worldSpacePixelPosition, direction, rayLength);

	}
	else if (isSphericalLight(lightIndex)) {

		// Spherical light
        return sampleDiskLight(lightIndex, lightsInfo[lightIndex].position, lightsInfo[lightIndex].size, viewSpacePixelPosition, worldSpacePixelPosition, launchIndex, false, samplesCountToUse, frameNumberToUse);

	}
	else if (isHardDirectionalLight(lightIndex)) {

		// Hard directional light
		float3 direction = -lightsInfo[lightIndex].direction;
		float rayLength = maxRayLength;

		return checkLightHit(lightIndex, worldSpacePixelPosition, direction, rayLength);

	}
	else if (isSoftDirectionalLight(lightIndex)) {

		// Soft directional light

		// Construct point light with unit radius in distance such that when viewed from shaded point, 
		// point light disk covers desired solid angle
		float3 lightPosition = worldSpacePixelPosition - (normalize(lightsInfo[lightIndex].direction) * (1.0f / tan(lightsInfo[lightIndex].size)));

        return sampleDiskLight(lightIndex, lightPosition, 1.0f, viewSpacePixelPosition, worldSpacePixelPosition, launchIndex, true, samplesCountToUse, frameNumberToUse);
    }

	return 1.0f;
}

inline void getWorldSpacePixelPosition(float2 uvs, float2 d, float linearDepth, out float3 viewSpacePixelPosition, out float4 worldSpacePixelPosition) {
	
	// Reconstruct world space position of this pixel

#ifdef USE_GBUFFER_DEPTH
	float aspectRatio = viewportDims.x / viewportDims.y;
	float3 rayDirection = normalize((d.x * invView[0].xyz * tanHalfFovY * aspectRatio) - (d.y * invView[1].xyz * tanHalfFovY) - invView[2].xyz);
	worldSpacePixelPosition = float4(invView[3].xyz + rayDirection * linearDepth, 1.0f);
	viewSpacePixelPosition = mul(worldSpacePixelPosition, view).xyz;
#else

	#ifdef USE_PRIMARY_RAYS

		RayDesc ray;
		ray.Origin = invView[3].xyz;

		// We negate the Z exis because the 'view' matrix is generated using a 
		// Right Handed coordinate system with Z pointing towards the viewer
		// The negation of Z axis is needed to get the rays go out in the direction away fromt he viewer.
		// The negation of Y axis is needed because the texel coordinate system, used in the UAV we write into using launchIndex
		// has the Y axis flipped with respect to the camera Y axis (0 is at the top and 1 at the bottom)
		float aspectRatio = viewportDims.x / viewportDims.y;
		ray.Direction = normalize((d.x * invView[0].xyz * tanHalfFovY * aspectRatio) - (d.y * invView[1].xyz * tanHalfFovY) - invView[2].xyz);

		ray.TMin = 0.001;
		ray.TMax = maxRayLength;

		PrimaryRayData hitData;
		hitData.depth = 0;

		TraceRay(gRtScene, 0 /*rayFlags*/, 0xFF, 0 /* ray index*/, hitProgramCount, 0, ray, hitData);

		worldSpacePixelPosition = float4(invView[3].xyz + ray.Direction * hitData.hitT, 1.0f);
		viewSpacePixelPosition = mul(worldSpacePixelPosition, view).xyz;
	#else
		viewSpacePixelPosition = getViewSpacePositionUsingDepth(uvs, linearDepth, screenUnprojection, screenUnprojection2) * float3(1, -1, 1);
		worldSpacePixelPosition = mul(float4(viewSpacePixelPosition, 1.0f), invView);
	#endif
#endif

}

[shader("raygeneration")]
void rayGen()
{

	// Figure out screen space coordinates (uvs) of this pixel
	uint2 launchIndex = DispatchRaysIndex().xy;
	float2 d = (((float2(launchIndex)+0.5f) * texelSizeRcp) * 2.f - 1.f);
	float2 uvs = (d + 1.0f) * 0.5f;

	// Load motion vector and linear depth
	float3 motionAndDepth = motionAndDepthBuffer.Load(int3(launchIndex, 0)).xyz;

	// Check if reverse reprojection succeeded for this pixel
	bool rejectReprojection = (motionAndDepth.x == INVALID_UVS.x && motionAndDepth.y == INVALID_UVS.y);
	
	// Figure out screen space coordinates of this pixel in previous frame (reverse reprojection)
	float2 previousUvs = rejectReprojection ? uvs : motionAndDepth.xy;
	int2 previousPos = int2(motionAndDepth.xy * viewportDims);

	// See if reprojection is identity (no movement)
	bool isIdentityReprojection = (previousUvs.x == uvs.x && previousUvs.y == uvs.y);

	// Read filtered variation measure from previous frames for all lights in this pass
	float4 inputFilteredVariation0To3 = inputFilteredVariationBuffer[0].Load(int3(launchIndex, 0));
	float4 inputFilteredVariation4To7 = inputFilteredVariationBuffer[1].Load(int3(launchIndex, 0));
	float4 inputFilteredVariation0To3Reprojected = isIdentityReprojection ? inputFilteredVariation0To3 : inputFilteredVariationBuffer[0].Load(int3(previousPos, 0));
	float4 inputFilteredVariation4To7Reprojected = isIdentityReprojection ? inputFilteredVariation4To7 : inputFilteredVariationBuffer[1].Load(int3(previousPos, 0));

	// Read filtered variation from lowest mip-level (an estimation of varation in the whole image)
	float4 inputFilteredVariationGlobal0To3 = inputFilteredVariationBuffer[0].Load(int3(0, 0, variationBufferBottomMipLevel));
	float4 inputFilteredVariationGlobal4To7 = inputFilteredVariationBuffer[1].Load(int3(0, 0, variationBufferBottomMipLevel));

	// Reconstruct world space position of this pixel
	float4 worldSpacePixelPosition;
	float3 viewSpacePixelPosition;
	getWorldSpacePixelPosition(uvs, d, motionAndDepth.z, viewSpacePixelPosition, worldSpacePixelPosition);

	// Declare output variables
	float4 visibility0To3 = float4(0.0f);
	float4 visibility4To7 = float4(0.0f);
	float4 outputVariation0To3 = float4(0.0f);
	float4 outputVariation4To7 = float4(0.0f);

#ifdef APPLY_SAMPLING_MATRIX
	// Skip calculation according to mask matrix for areas with zero variation
	uint4x4 maskMatrix = int4x4(0, 3, 2, 1, 2, 1, 0, 3, 0, 3, 2, 1, 1, 2, 0, 3);
	int2 maskEntryIds = int2(fmod(float2(launchIndex / cullingBlockSize), float2(4.0f)));
	uint maskEntry = maskMatrix[maskEntryIds.x][maskEntryIds.y];

	bool canSkipShadowSampling = (maskEntry != frameNumber);
#endif

	// Resolve visibility for all lights (in this pass)
	[unroll]
	for (int lightIndex = 0; lightIndex < _LIGHT_COUNT; lightIndex++)
	{

		float visibilityResult = 0.0f;

		// Load variation history and sample counts used in previous frames for this light
		int4 pastSampleCounts;
		float4 variationHistory = decodeVariationAndSampleCount(outputVariationAndSampleCountCache[lightIndex][launchIndex], pastSampleCounts);
		
		// Load visibility cache (both reprojected and from this pixels's position) for this light
		float4 visibilityHistoryReprojected = inputVisibilityCache[lightIndex].SampleLevel(bilinearSampler, previousUvs, 0);
		float4 visibilityHistory = isIdentityReprojection ? visibilityHistoryReprojected : inputVisibilityCache[lightIndex].SampleLevel(bilinearSampler, uvs, 0);

		float inputFilteredVariation = unpackFrom4Texture(inputFilteredVariation0To3, inputFilteredVariation4To7, lightIndex);
		float inputFilteredVariationReprojected = unpackFrom4Texture(inputFilteredVariation0To3Reprojected, inputFilteredVariation4To7Reprojected, lightIndex);
		float inputFilteredVariationGlobal = unpackFrom4Texture(inputFilteredVariationGlobal0To3, inputFilteredVariationGlobal4To7, lightIndex);

		// Check sampling matrix if this pixel can be culled in this frame
#ifdef APPLY_SAMPLING_MATRIX
		bool skipShadowSampling = (!rejectReprojection && ((inputFilteredVariation == 0.0f && inputFilteredVariationReprojected == 0.0f) && canSkipShadowSampling));
#else
		bool skipShadowSampling = false;
#endif

		int samplesCountToUse = lightSamplesCount;

		if (skipShadowSampling)
        {
            samplesCountToUse = 0;
        } else {

#ifdef USE_ADAPTIVE_SAMPLING

			// Adaptive sampling
			int previousSampleCount = pastSampleCounts.r;
            samplesCountToUse = previousSampleCount;

			float currentFilteredVariation = (inputFilteredVariation + (dot(variationHistory, float4(0.25f)))) * 0.5f;

			if (currentFilteredVariation < variationThreshold)
			{
				// Try decreasing number of samples (only if it was stable in 3 previous frames)
				if (pastSampleCounts.r == pastSampleCounts.g &&
					pastSampleCounts.r == pastSampleCounts.b)
				{
					samplesCountToUse = previousSampleCount - 1;
				}

			}
			else if (currentFilteredVariation > variationThreshold)
			{
				// Increase number of samples
				samplesCountToUse = previousSampleCount + 1;
			}

			// Apply maximal sample count when reverse reprojection fails
			if (rejectReprojection) samplesCountToUse = maxLightSamplesCount;

			// Clamp sample count to selected limits
			samplesCountToUse = clamp(samplesCountToUse, 1, maxLightSamplesCount);

#endif

			// Progressively lower sample count if global variation is too high
#ifdef LIMIT_GLOBAL_SAMPLE_COUNT
			if (inputFilteredVariationGlobal > inputFilteredVariationGlobalThreshold)
			{
				samplesCountToUse = lerp(samplesCountToUse, 1, (inputFilteredVariationGlobal - inputFilteredVariationGlobalThreshold) * inputFilteredVariationGlobalFalloffSpeed);
			}
#endif
		}

		// Sample the light with selected number of samples
        if (samplesCountToUse == 0) 
            visibilityResult = visibilityHistoryReprojected.r;
		else 
            visibilityResult = sampleLight(lightIndex, worldSpacePixelPosition.xyz, viewSpacePixelPosition, launchIndex, samplesCountToUse, rejectReprojection ? 0 : frameNumber);

		// Evaluate new visibility history entry. Use current result if reverse reprojection failed
		float4 newHistory = rejectReprojection ? float4(visibilityResult) : float4(visibilityResult, visibilityHistoryReprojected.r, visibilityHistoryReprojected.g, visibilityHistoryReprojected.b);

		// Store filtered visibility to correct position in output texture for this light
		// Note: manually inlined 'pack4ToTexture' function to prevent compiler validation errors
		//pack4ToTexture(lightIndex, calculateVisibilityFromHistory(newHistory), visibility0To3, visibility4To7);
		float filteredVisibility = calculateVisibilityFromHistory(newHistory);
		if (lightIndex == 0) visibility0To3.r = filteredVisibility;
		else if (lightIndex == 1) visibility0To3.g = filteredVisibility;
		else if (lightIndex == 2) visibility0To3.b = filteredVisibility;
		else if (lightIndex == 3) visibility0To3.a = filteredVisibility;
		else if (lightIndex == 4) visibility4To7.r = filteredVisibility;
		else if (lightIndex == 5) visibility4To7.g = filteredVisibility;
		else if (lightIndex == 6) visibility4To7.b = filteredVisibility;
		else if (lightIndex == 7) visibility4To7.a = filteredVisibility;

		// Calculate variation from visibility history in this pixel (not reprojected)
		float filteredVariation = calculateFilteredVariation(float4(visibilityHistory.rgb, visibilityResult));
		variationHistory.g = variationHistory.r;
		variationHistory.b = variationHistory.g;
		variationHistory.a = variationHistory.b;
		variationHistory.r = filteredVariation;

		// Store visibility, variation measure and samples count caches for this light
		outputVisibilityCache[lightIndex][launchIndex] = newHistory;
		outputVariationAndSampleCountCache[lightIndex][launchIndex] = encodeVariationAndSampleCount(variationHistory, int4(samplesCountToUse, pastSampleCounts.r, pastSampleCounts.g, pastSampleCounts.b));

		// Store filtered variation to correct position in output texture for this light
		// Note: manually inlined 'pack4ToTexture' function to prevent compiler validation errors
		//pack4ToTexture(lightIndex, filteredVariation, outputVariation0To3, outputVariation4To7);
		if (lightIndex == 0) outputVariation0To3.r = filteredVariation;
		else if (lightIndex == 1) outputVariation0To3.g = filteredVariation;
		else if (lightIndex == 2) outputVariation0To3.b = filteredVariation;
		else if (lightIndex == 3) outputVariation0To3.a = filteredVariation;
		else if (lightIndex == 4) outputVariation4To7.r = filteredVariation;
		else if (lightIndex == 5) outputVariation4To7.g = filteredVariation;
		else if (lightIndex == 6) outputVariation4To7.b = filteredVariation;
		else if (lightIndex == 7) outputVariation4To7.a = filteredVariation;

	}

	// Store filtered visibility and variation for all lights in this pass
	outputFilteredVisibilityBuffer[0][launchIndex] = visibility0To3;
	outputFilteredVisibilityBuffer[1][launchIndex] = visibility4To7;
	outputFilteredVariationBuffer[0][launchIndex] = outputVariation0To3;
	outputFilteredVariationBuffer[1][launchIndex] = outputVariation4To7;

}

