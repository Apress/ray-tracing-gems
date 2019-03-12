struct ParticleSample {
  float t;
  uint  id;
};

const int PARTICLE_BUFFER_SIZE = 31;    // 31 for Turing, 255 for Volta

struct PerRayData {
  int           tail;                   // End index of the array
  int           pad;
  ParticleSample      particles[PARTICLE_BUFFER_SIZE];  // Array
};

rtDeclareVariable(rtObject,      top_object, , );
rtDeclareVariable(float,         radius, ,  );
rtDeclareVariable(float3,        volume_bbox_min, , );
rtDeclareVariable(float3,        volume_bbox_max, , );
rtBuffer<uchar4, 2>              output_buffer;

RT_PROGRAM raygen_program()
{
  optix::Ray ray;
  PerRayData prd;

  generate_ray(launch_index, camera);   // Pinhole camera or similar
  optix::Aabb aabb(volume_bbox_min, volume_bbox_max);

  float tenter, texit;
  intersect_Aabb(ray, aabb, tenter, texit);

  float3 result_color = make_float3(0.f);
  float result_alpha = 0.f;

  if (tenter < texit)
  {
    const float slab_spacing =
          PARTICLE_BUFFER_SIZE * particlesPerSlab * radius;
    float tslab = 0.f;

    while (tslab < texit && result_alpha < 0.97f)
    {
      prd.tail = 0;
      ray.tmin = fmaxf(tenter, tslab);
      ray.tmax = fminf(texit, tslab + slabWidth);

      if (ray.tmax > tenter)
      {
        rtTrace(top_object, ray, prd);

        sort(prd.particles, prd.tail);

        // Integrate depth-sorted list of particles.
        for (int i=0; i<prd.tail; i++) {
          float drbf = evaluate_rbf(prd.particles[i]);
          float4 color_sample = transfer_function(drbf); // return RGBA
          float alpha_1msa = color_sample.w * (1.0 - result_alpha);
          result_color += alpha_1msa * make_float3(
                color_sample.x, color_sample.y, color_sample.z);
          result_alpha += alpha_1msa;
        }
      }
      tslab += slab_spacing;
    }
  }

  output_buffer[launch_index] = make_color( result_color ) );
}
