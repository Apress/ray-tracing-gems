// This closest-hit shader code snippet skips the shading of
// transparent surfaces when the incident ray has crossed
// through a user-defined maximum number of transparent surfaces,
// proceeding instead by shooting a transmission ray and
// continuing as though there had been no ray/surface intersection.},

struct PerRayData_radiance {
  float3 result;     // Final shaded surface color
  int transcnt;      // Transmission ray surface count/depth
  int depth;         // Current ray recursion depth
  // ...
}

rtDeclareVariable(PerRayData_radiance, prd, rtPayload, );

RT_PROGRAM void closest_hit_shader( ... ) {
  // Skipping boilerplate closest-hit shader material here ...

  // Do not shade transparent surface if the maximum transcnt has been reached.
  if ((opacity < 1.0) && (transcnt < 1)) {
    // Spawn transmission ray; shading behaves as if there was no intersection.
    PerRayData_radiance new_prd;
    new_prd.depth = prd.depth; // Do not increment recursion depth.
    new_prd.transcnt = prd.transcnt - 1;
    // Set/update various other properties of the new ray.

    // Shoot the new transmission ray and return its color as if
    // there had been no intersection with this transparent surface.
    Ray trans_ray = make_Ray(hit_point, ray.direction, radiance_ray_type, scene_epsilon, RT_DEFAULT_MAX);
    rtTrace(root_object, trans_ray, new_prd);
  }

  // Otherwise, continue shading this transparent surface hit point normally ...

  // Continue with typical closest-hit shader contents ...
  prd.result = result; // Pass the resulting color back up the tree.
}
