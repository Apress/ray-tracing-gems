// If we are going from air to glass or glass to air,
// choose the correct index of refraction ratio.
bool isBackFace = (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE);
float ior = isBackFace ? iorGlass / iorAir : iorAir / iorGlass;

RayDesc refractionRay;
refractionRay.Origin = worldPosition;
refractionRay.Direction = refract(worldRayDir, worldNormal, ior);
