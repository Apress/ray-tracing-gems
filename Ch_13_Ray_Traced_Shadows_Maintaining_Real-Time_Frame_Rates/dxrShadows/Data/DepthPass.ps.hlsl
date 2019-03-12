__import DefaultVS;
__import ShaderCommon;
__import Shading;

void main(VertexOut vOut)
{
    prepareShadingData(vOut, gMaterial, gCamera.posW);
}