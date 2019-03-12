// Kernel code describing the lighting integration. Improved version.

Ray r = initRay(texelOrigin, randomDirection);

float3 outRadiance = 0;
float3 pathThroughput = 1;
while (pathVertexCount++ < maxDepth) {
    PrimaryRayData rayData;
    TraceRay(r, rayData);
    
    if (!rayData.hasHitAnything) {
        outRadiance += pathThroughput * getSkyDome(r.Direction);
        break;
    }
    
    float3 Pos = r.Origin + r.Direction * rayData.hitT;
    float3 L = sampleLocalLighting(Pos, rayData.Normal);
    
    pathThroughput *= rayData.albedo;
    outRadiance += pathThroughput * (L + rayData.emissive);
    
    r.Origin = Pos;
    r.Direction = sampleCosineHemisphere(rayData.Normal);
}

return outRadiance;
