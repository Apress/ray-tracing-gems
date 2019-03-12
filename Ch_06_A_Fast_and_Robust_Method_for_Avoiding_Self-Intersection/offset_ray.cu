// Implementation of our method as described in Section 6.2.2.

constexpr float origin()      { return 1.0f / 32.0f; }
constexpr float float_scale() { return 1.0f / 65536.0f; }
constexpr float int_scale()   { return 256.0f; }

// Normal points outward for rays exiting the surface, else is flipped.
float3 offset_ray(const float3 p, const float3 n)
{
  int3 of_i(int_scale() * n.x, int_scale() * n.y, int_scale() * n.z);

  float3 p_i(
     int_as_float(float_as_int(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
     int_as_float(float_as_int(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
     int_as_float(float_as_int(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

  return float3(fabsf(p.x) < origin() ? p.x+float_scale()*n.x : p_i.x,
                fabsf(p.y) < origin() ? p.y+float_scale()*n.y : p_i.y,
                fabsf(p.z) < origin() ? p.z+float_scale()*n.z : p_i.z);
}