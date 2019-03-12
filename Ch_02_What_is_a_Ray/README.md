# Ray Definition

The code mentioned in "What is a Ray?" in _Ray Tracing Gems_ is minimal, just this snippet from DXR's definition of a ray, so we give it here directly:

	struct RayDesc
	{
		float3 Origin;
		float  TMin;
		float3 Direction;
		float  TMax;
	};
