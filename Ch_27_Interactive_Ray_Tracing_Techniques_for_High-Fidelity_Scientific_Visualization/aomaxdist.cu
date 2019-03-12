// This closest-hit shader code snippet skips shading of
// transparent surfaces when the incident ray has crossed
// through a user-defined maximum number of transparent surfaces,
// proceeding instead by shooting a transmission ray and
// continuing as though there had been no ray/surface intersection.},

struct PerRayData_radiance {
  float3 result;     // Final shaded surface color
  // ...
}

struct PerRayData_shadow {
  float3 attenuation;
};

rtDeclareVariable(PerRayData_radiance, prd, rtPayload, );
rtDeclareVariable(PerRayData_shadow, prd_shadow, rtPayload, );

rtDeclareVariable(float, ao_maxdist, , ); // max AO occluder distance

static __device__ float3 shade_ambient_occlusion(float3 hit, float3 N, float aoimportance) {
  // Skipping boilerplate AO shadowing material here ...

  for (int s=0; s<ao_samples; s++) {
    Ray aoray;
    // Skipping boilerplate AO shadowing material here ...
    aoray = make_Ray(hit, dir, shadow_ray_type, scene_epsilon, ao_maxdist);

    shadow_prd.attenuation = make_float3(1.0f);
    rtTrace(root_shadower, ambray, shadow_prd);
    inten += ndotambl * shadow_prd.attenuation;
  }

  return inten * lightscale;
}

RT_PROGRAM void closest_hit_shader( ... ) {
  // Skipping boilerplate closest-hit shader material here ...

  // Add ambient occlusion diffuse lighting, if enabled.
  if (AO_ON && ao_samples > 0) {
    result *= ao_direct;
    result += ao_ambient * col * p_Kd * shade_ambient_occlusion(hit_point, N, fogmod * p_opacity);
  }

  // Continue with typical closest-hit shader contents ...

  prd.result = result; // Pass the resulting color back up the tree.
}
