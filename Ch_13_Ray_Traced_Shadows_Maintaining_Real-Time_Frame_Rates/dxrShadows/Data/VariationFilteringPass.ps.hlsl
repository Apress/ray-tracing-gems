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

#ifndef OPERATOR
	#define OPERATOR MAXIMUM
#endif

Texture2D inputBuffer[MAX_LIGHTS_PER_PASS_QUARTER];

struct PsOut
{
    float4 variationLevel0to3 : SV_TARGET0; //< variation level buffers for 4 lights (0 to 3) packed into single texture
    float4 variationLevel4to7 : SV_TARGET1; //< variation level buffers for 4 lights (4 to 7) packed into single texture
};

inline float4 blurPass(int lightBufferIndex, int2 pos) {

	const int halfKernelSize = 4;

	int2 offset;
	float4 result = float4(0.0f);
	float tapWeight = 1.0f / float(halfKernelSize * 2 + 1);

	[unroll]
	for (int i = -halfKernelSize; i <= halfKernelSize; ++i)
	{
		if (BLUR_PASS_DIRECTION == BLUR_HORIZONTAL)
			offset = pos + int2(i, 0);
		else
			offset = pos + int2(0, i);

		float4 tapVariationLevel = inputBuffer[lightBufferIndex].Load(int3(offset, 0));
        
        result += tapVariationLevel * tapWeight;
    }

	return result;
}

inline float4 distributeMaxPass(int lightBufferIndex, int2 pos) {

	const int halfKernelSize = 2;

	int2 offset;
	float4 result = float4(0.0f);

	[unroll]
	for (int i = -halfKernelSize; i <= halfKernelSize; ++i)
	{
		if (BLUR_PASS_DIRECTION == BLUR_HORIZONTAL)
			offset = pos + int2(i, 0);
		else
			offset = pos + int2(0, i);

        float4 tapVariationLevel = inputBuffer[lightBufferIndex].Load(int3(offset, 0));
        
        result = max(result, tapVariationLevel);
    }

	return result;
}

inline float4 visibilityFilteringPass(int lightBufferIndex, int2 pos) {
    
    if (OPERATOR == MAXIMUM)
    {
        return distributeMaxPass(lightBufferIndex, pos);
    }
    else
    {
        float4 input = inputBuffer[lightBufferIndex].Load(int3(pos, 0));

        return blurPass(lightBufferIndex, pos);        
    }

}

PsOut main(float2 uvs : TEXCOORD, float4 pos : SV_POSITION) : SV_TARGET
{
    PsOut output;
    
	if (_LIGHT_COUNT_PASS_0 > 0) output.variationLevel0to3 = visibilityFilteringPass(0, int2(pos.xy));
	if (_LIGHT_COUNT_PASS_1 > 0) output.variationLevel4to7 = visibilityFilteringPass(1, int2(pos.xy));

	return output;
}
