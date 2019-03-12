
#define MAX_LIGHTS_PER_PASS 8
#define MAX_LIGHTS_PER_PASS_QUARTER 2

#define BLUR_HORIZONTAL 1
#define BLUR_VERTICAL 2

#define MAXIMUM 1
#define AVERAGE 2

#define INVALID_LIGHT_INDEX 65535
#define INVALID_UVS float2(-10.0f, -10.0f)

#define POINT_LIGHT 1
#define SPHERICAL_LIGHT 2
#define DIRECTIONAL_HARD_LIGHT 3
#define DIRECTIONAL_SOFT_LIGHT 4

struct ShadowsLightInfo {
	float3 position;
	float3 direction;
	float size;
	int type;
};