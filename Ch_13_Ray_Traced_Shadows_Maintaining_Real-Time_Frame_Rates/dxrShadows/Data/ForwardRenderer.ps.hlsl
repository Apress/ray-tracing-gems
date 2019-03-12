__import ShaderCommon;
__import DefaultVS;
__import Effects.CascadedShadowMap;
__import Shading;
__import Helpers;
__import BRDF;

#include "CommonUtils.hlsl"

#ifndef _LIGHT_COUNT
#define _LIGHT_COUNT 1 //< Default to one light
#endif

#define MAX_LIGHT_SOURCES 16

layout(binding = 0) cbuffer PerFrameCB : register(b0)
{
    CsmData gCsmData;
    float4x4 camVpAtLastCsmUpdate;
    float2 gRenderTargetDim;
    float gOpacityScale;
	Texture2D gVisibilityBuffers[MAX_LIGHTS_PER_PASS];
	int visibilityBufferToDisplay;
    LightData lights[MAX_LIGHT_SOURCES];
};

layout(set = 1, binding = 1) SamplerState gSampler;

struct MainVsOut
{
    VertexOut vsData;
    float shadowsDepthC : DEPTH;
};

MainVsOut vs(VertexIn vIn)
{
    MainVsOut vsOut;
    vsOut.vsData = defaultVS(vIn);
    vsOut.shadowsDepthC = mul(float4(vsOut.vsData.posW, 1), camVpAtLastCsmUpdate).z;
    return vsOut;
}

inline float getRtShadowValue(float4 packedVisibilityBuffer0To3, float4 packedVisibilityBuffer4To7, uint lightIndex, float2 pos) {

#ifdef USE_SHADOW_MAPPING
	return gVisibilityBuffers[lightIndex].Load(int3(pos, 0)).r;
#else
	if (lightIndex == 0) return packedVisibilityBuffer0To3.x;
	else if (lightIndex == 1) return packedVisibilityBuffer0To3.y;
	else if (lightIndex == 2) return packedVisibilityBuffer0To3.z;
	else if (lightIndex == 3) return packedVisibilityBuffer0To3.w;
	else if (lightIndex == 4) return packedVisibilityBuffer4To7.x;
	else if (lightIndex == 5) return packedVisibilityBuffer4To7.y;
	else if (lightIndex == 6) return packedVisibilityBuffer4To7.z;
	else if (lightIndex == 7) return packedVisibilityBuffer4To7.w;

    return 1.0f;
#endif
}

float4 ps(MainVsOut vOut, float4 pixelCrd : SV_POSITION) : SV_TARGET
{
    ShadingData sd = prepareShadingData(vOut.vsData, gMaterial, gCamera.posW);

    float4 finalColor = float4(0, 0, 0, 1);
	float4 packedVisibilityBuffer0To3 = float4(0.0f);
	float4 packedVisibilityBuffer4To7 = float4(0.0f);

#ifdef USE_RAYTRACED_SHADOWS
	packedVisibilityBuffer0To3 = gVisibilityBuffers[0].Load(int3(vOut.vsData.posH.xy, 0)).rgba;
	packedVisibilityBuffer4To7 = gVisibilityBuffers[1].Load(int3(vOut.vsData.posH.xy, 0)).rgba;
#endif

    [unroll]
    for (uint l = 0; l < _LIGHT_COUNT; l++)
    {

        float shadowFactor = getRtShadowValue(packedVisibilityBuffer0To3, packedVisibilityBuffer4To7, l, vOut.vsData.posH.xy);

        if (visibilityBufferToDisplay != INVALID_LIGHT_INDEX)
        {
            shadowFactor = visibilityBufferToDisplay == l ? shadowFactor : 1.0f;
        }

#ifdef DISPLAY_SHADOWS_ONLY
		if (visibilityBufferToDisplay != INVALID_LIGHT_INDEX) {
			finalColor.rgb += visibilityBufferToDisplay == l ? shadowFactor : 0.0f;
		} else {
			finalColor.rgb += (shadowFactor / float(_LIGHT_COUNT));
		}
#else 

#ifdef SHADOWS_OFF
			shadowFactor = 1.0f;
#endif
        
        if (shadowFactor > 0.0f)
        {
			// Shade if light is at least partially visible
            finalColor.rgb += evalMaterial(sd, lights[l], shadowFactor).color.rgb;
        }
#endif

    }

#ifndef DISPLAY_SHADOWS_ONLY

    // Add the emissive component
    finalColor.rgb += sd.emissive;

    // Add light-map
    finalColor.rgb += sd.diffuse * sd.lightMap.rgb;

#endif

    return finalColor;

}
