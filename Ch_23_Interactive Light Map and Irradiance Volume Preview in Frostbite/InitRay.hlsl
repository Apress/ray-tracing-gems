// Kernel code describing a simple light integration.

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
    
    outRadiance += pathThroughput * rayData.emissive;
    
    r.Origin = r.Origin + r.Direction * rayData.hitT;
    r.Direction = sampleHemisphere(rayData.Normal);
    pathThroughput *= rayData.albedo * dot(r.Direction,rayData.Normal);
}

return outRadiance;
