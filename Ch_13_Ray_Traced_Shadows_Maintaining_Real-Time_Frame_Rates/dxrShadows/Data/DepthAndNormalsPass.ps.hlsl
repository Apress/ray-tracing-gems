__import DefaultVS;
__import ShaderCommon;
__import Shading;
#include "CommonUtils.hlsl"

cbuffer GBufferPassCB
{
	float4x4 normalMatrix;
	float farOverNear;
	float maxReprojectionDepthDifference;
};

Texture2D previousDepthBuffer;
SamplerState bilinearSampler;

struct PsOut
{
	float4 motionAndDepth : SV_TARGET0; // xy - previous frame uv's, z - current frame depth
	float4 normal : SV_TARGET1;
};

PsOut main(VertexOut vOut)
{
	PsOut output;

    ShadingData sd = prepareShadingData(vOut, gMaterial, gCamera.posW);

    // Skip transparent objects in G-Buffer pass
    if (sd.opacity < 1.0f)
    {
        discard;
    }

	float3 pixelViewSpacePosition = mul(float4(vOut.posW, 1.0f), gCamera.viewMat).xyz;
	float currentLinearDepth = length(pixelViewSpacePosition);

	float previousReprojectedLinearDepth01 = getNormalizedLinearDepth(vOut.prevPosH.z / vOut.prevPosH.w, farOverNear);
	float2 previousUvs = ((vOut.prevPosH.xy / vOut.prevPosH.w) * float2(0.5f, -0.5f)) + 0.5f;

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

	output.motionAndDepth = float4(previousUvs, currentLinearDepth, 0.0f);
	output.normal = float4(normalize(mul(float4(normalize(vOut.normalW), 0.0f), normalMatrix).xyz), 0.0f);

	return output;
}
