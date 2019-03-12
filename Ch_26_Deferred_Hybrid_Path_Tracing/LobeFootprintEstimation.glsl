mat2 footPrint;
// "Bounce-off" direction
footPrint[0] = normalize(ssNormal.xy);
// Lateral direction
footPrint[1] = vec2(footPrint[0].y, -footPrint[0].x);

vec2 footprintScale = vec2(roughness*rayLength / (rayLength + sceneZ));

// On a convex surface, the estimated footprint is smaller.
vec3 plane0 = cross(ssV, ssNormal);
vec3 plane1 = cross(plane0, ssNormal);
// estimateCurvature(...) calculates the depth gradient from the 
// G-buffer's depth along the directions stored in footPrint.
vec2 curvature = estimateCurvature(footPrint, plane0, plane1);
curvature = 1.0 / (1.0 + CURVATURE_SCALE*square(ssNormal.z)*curvature);
footPrint[0] *= curvature.x;
footPrint[1] *= curvature.y;

// Ensure constant scale across different camera lenses.
footPrint *= KERNEL_FILTER / tan(cameraFov * 0.5);

// Scale according to NoV proportional lobe distortions. NoV contains
// the saturated dot product of the view vector and surface normal
footPrint[0] /= (1.0 - ELONGATION) + ELONGATION * NoV;
footPrint[1] *= (1.0 - SHRINKING) + SHRINKING * NoV;

for (i : each sample)
{
    vec2 samplingPosition = fragmentCenter + footPrint * sample[i];
    // ...
}
