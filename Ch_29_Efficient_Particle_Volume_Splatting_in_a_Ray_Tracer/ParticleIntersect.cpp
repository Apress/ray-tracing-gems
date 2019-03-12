rtDeclareVariable(ParticleSample,hit_particle,attribute hit_particle,);

RT_PROGRAM void particle_intersect( int primIdx )
{
  const float3 center = make_float3(particles_buffer[primIdx]);
  const float t = length(center - ray.origin);
  const float3 sample_pos = ray.origin + ray.direction * t;
  const float3 offset = center - sample_pos;
  if ( dot(offset,offset) < radius * radius && 
        rtPotentialIntersection(t) )
  {
    hit_particle.t = t;
    hit_particle.id = primIdx;
    rtReportIntersection( 0 );
  }
}

RT_PROGRAM void any_hit()
{
  if (prd.tail < PARTICLE_BUFFER_SIZE) {
    prd.particles[prd.tail++] = hit_particle;
    rtIgnoreIntersection();
  }
}
