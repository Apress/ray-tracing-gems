// This example code snippet makes viewer-facing surfaces appear
// completely transparent while leaving surfaces seen edge-on
// more visible and opaque.  This type of rendering is extremely
// useful to facilitate views into the interior of crowded
// scenes, such as densely packed biomolecular complexes.},

RT_PROGRAM void closest_hit_shader( ... ) {
  // Skipping boilerplate closest-hit shader material here ...

  // Exemplary simplified placeholder for typical transmission ray launch code
  if (alpha < 0.999f) {
    // Emulate Tachyon/Raster3D's angle-dependent surface opacity, if enabled.
    if (transmode) {
      alpha = 1.0f + cosf(3.1415926f * (1.0f-alpha) * dot(N, ray.direction));
      alpha = alpha*alpha * 0.25f;
    }
    result *= alpha; // Scale down current lighting by any new transparency.

    // Skipping boilerplate code to prepare a new transmission ray ...
    rtTrace(root_object, trans_ray, new_prd);
  }
  result += (1.0f - alpha) * new_prd.result;

  // Continue with typical closest-hit shader contents ...

  prd.result = result; // Pass the resulting color back up the tree.
}
