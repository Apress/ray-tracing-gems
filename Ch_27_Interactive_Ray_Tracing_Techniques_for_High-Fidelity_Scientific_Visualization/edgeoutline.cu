// This example code snippet adds a dark outline on the edges
// of geometry to help accentuate objects that are packed
// closely together and may not otherwise be visually distinct.},

struct PerRayData_radiance {
  float3 result;     // Final shaded surface color
  // ...
}

rtDeclareVariable(PerRayData_radiance, prd, rtPayload, );

// Example of instantiating a shader with outlining enabled ...
RT_PROGRAM void closest_hit_shader_outline( ... ) {
  // Skipping boilerplate closest-hit shader material here ...

  // Add edge shading, if applicable.
  if (outline > 0.0f) {
    float edgefactor = dot(N, ray.direction);
    edgefactor *= edgefactor;
    edgefactor = 1.0f - edgefactor;
    edgefactor = 1.0f - powf(edgefactor, (1.0f - outlinewidth) * 32.0f);
    float outlinefactor = __saturatef((1.0f - outline) + (edgefactor * outline));
    result *= outlinefactor;
  }

  // Continue with typical closest-hit shader contents ...

  prd.result = result; // Pass the resulting color back up the tree.
}
