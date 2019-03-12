__import ShaderCommon;
__import Shading;
#include "CommonUtils.hlsl"


cbuffer MotionVectorsPassCB
{
    float4x4 motionVectorsMatrix; //< (invView * previousViewProjectionMatrix)
    float4 screenUnprojection;
    float4 screenUnprojection2;
	float2 texelSizeRcp;
    float farOverNear;
    float maxReprojectionDepthDifference;
};

Texture2D currentDepthBuffer;
Texture2D previousDepthBuffer;
SamplerState bilinearSampler;

struct PsOut
{
	float4 motionAndDepth : SV_TARGET0; // xy - previous frame uv's, z - current frame depth
	float4 normal : SV_TARGET1;
};

PsOut main(float2 uvs : TEXCOORD, float4 pos : SV_POSITION)
{
	PsOut output;

	float currentLinearDepth01 = getNormalizedLinearDepth(currentDepthBuffer.Load(int3(pos.xy, 0)).r, farOverNear);
    
    float3 pixelViewSpacePosition = getViewSpacePositionUsingDepth(uvs, currentLinearDepth01, screenUnprojection, screenUnprojection2) * float3(1, -1, 1);

    float4 previousPixelScreenSpacePosition = mul(float4(pixelViewSpacePosition, 1.0f), motionVectorsMatrix);
    previousPixelScreenSpacePosition.xyz /= previousPixelScreenSpacePosition.w;
    float previousReprojectedLinearDepth01 = getNormalizedLinearDepth(previousPixelScreenSpacePosition.z, farOverNear);

    float2 previousUvs = (previousPixelScreenSpacePosition.xy * float2(0.5f, -0.5f)) + 0.5f;

    // Discard invalid pixels
    //  1. Samples outside of the frame
    if (previousUvs.x > 1.0f || previousUvs.x < 0.0f)
        previousUvs = INVALID_UVS;

    if (previousUvs.y > 1.0f || previousUvs.y < 0.0f)
        previousUvs = INVALID_UVS;

    // 2. Samples with different depth
    float previousSampledLinearDepth01 = getNormalizedLinearDepth(previousDepthBuffer.Sample(bilinearSampler, previousUvs).r, farOverNear);
    if (abs(1.0f - (previousReprojectedLinearDepth01 / previousSampledLinearDepth01)) > maxReprojectionDepthDifference)
        previousUvs = INVALID_UVS;

    output.motionAndDepth = float4(previousUvs, currentLinearDepth01, 0.0f);

	// Calculate normal from depth
	output.normal = float4(calculateNormal(currentDepthBuffer, uvs, pos.xy, farOverNear, screenUnprojection, screenUnprojection2, texelSizeRcp), 0.0f);

	return output;
}
