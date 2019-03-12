// This header includes a simple DXR sphere intersection routine.  The computed
//      sphere attributes are not well thought out -- returning the sphere center
//      is probably not the ideal return value, but it was simple and provided me
//      me with everything I needed for this simple demo

// This include assumes global shared variables of the following format have been declared:
//      shared Buffer<float3> gAABBData;  

// The attributes our sphere intersection returns to hit shaders
struct SphereAttribs
{
	float3 sphereCtr;
};

[shader("intersection")]
void SphereIntersect()
{
	// Get data about the sphere
	uint sphereNum = PrimitiveIndex();
	float3 boxMin = gAABBData[2 * sphereNum].rgb;
	float3 boxMax = gAABBData[2 * sphereNum + 1].rgb;
	float3 center = (boxMin + boxMax) * 0.5f;
	float  radius = abs((boxMax.x - boxMin.x) * 0.5f);

	// Get data about the ray
	float3 orig = WorldRayOrigin();
	float3 dir = WorldRayDirection();

	// method zero: basic quadratic formula; one: Press' more stable quadratic solution
	// two: Hearn & Baker's small spheres intersector. three: Press + Hearn together
	// four: use method three, but do not normalize the direction.
	int method = 5;
	if (method < 2) {

		// Compute a, b, c, for quadratic in ray-sphere intersection
		//    (Math can be simplified; left in its entirety for clarity.)
		float3 toCtr = orig - center;
		float a = dot(dir, dir);
		float b = 2.0f*dot(dir, toCtr);
		float c = dot(toCtr, toCtr) - radius * radius;

		// Checks if sqrt(b^2 - 4*a*c) in quadratic equation has real answer
		float discriminant = b * b - 4.0f*a*c;
		if (discriminant >= 0.0f)
		{
			float sqrtVal = sqrt(discriminant);
			SphereAttribs sphrAttr = { center };
			if (method == 0) {
				// method 0
				ReportHit((-b - sqrtVal) / (2.0f * a), 0, sphrAttr);
				// more distant hit - not needed if we know we will intersect with the outside of the sphere
				ReportHit((-b + sqrtVal) / (2.0f * a), 0, sphrAttr);
			}
			else {
				// method 1
				// from Press, William H., Saul A. Teukolsky, William T. Vetterling, and Brian P. Flannery, 
				// "Numerical Recipes in C," Cambridge University Press, 1992.
				float q = (b >= 0) ? (-0.5*(b + sqrtVal)) : (-0.5*(b - sqrtVal));

				// we don't bother testing for division by zero
				ReportHit(c / q, 0, sphrAttr);
				// more distant hit - not needed if we know we will intersect with the outside of the sphere
				ReportHit(q / a, 0, sphrAttr);
			}
		}
	}
	else if (method < 4) {
		// Hearn and Baker equation 10-72 for when radius^2 << distance between origin and center
		// Also at https://www.cg.tuwien.ac.at/courses/EinfVisComp/Slides/SS16/EVC-11%20Ray-Tracing%20Slides.pdf
		// In our particular application we don't need to normalize here, as the direction is already normalized (or distance doesn't matter).
		// If not normalized, this next line should be uncommented.
		//dir = normalize(dir);
		float3 deltap = center - orig;
		float ddp = dot(dir, deltap);
		float deltapdot = dot(deltap, deltap);

		float3 remedyTerm = deltap - ddp * dir;
		float discriminant = radius * radius - dot(remedyTerm, remedyTerm);
		if (discriminant >= 0.0f)
		{
			float sqrtVal = sqrt(discriminant);
			SphereAttribs sphrAttr = { center };
			if (method == 2) {
				// method 2
				ReportHit(ddp - sqrtVal, 0, sphrAttr);
				// more distant hit - not needed if we know we will intersect with the outside of the sphere
				ReportHit(ddp + sqrtVal, 0, sphrAttr);
			}
			else {
				// method 3
				// include Press, William H., Saul A. Teukolsky, William T. Vetterling, and Brian P. Flannery, 
				// "Numerical Recipes in C," Cambridge University Press, 1992.
				float q = (ddp >= 0) ? (ddp + sqrtVal) : (ddp - sqrtVal);

				// we don't bother testing for division by zero
				ReportHit((deltapdot - radius * radius) / q, 0, sphrAttr);
				// more distant hit - not needed if we know we will intersect with the outside of the sphere
				ReportHit(q, 0, sphrAttr);
			}
		}
	}
	else if (method == 4) {
		// Hearn and Baker equation 10-72 for when radius^2 << distance between origin and center
		// Also at https://www.cg.tuwien.ac.at/courses/EinfVisComp/Slides/SS16/EVC-11%20Ray-Tracing%20Slides.pdf
		// In our particular application we don't need to normalize here, as the direction is already normalized (or distance doesn't matter).
		// If not normalized, this next line should be uncommented.
		//dir = normalize(dir);
		float3 f = orig - center;
		float r2 = radius * radius;
		float a = dot(dir, dir);
		float b = 2.0f * dot(f, dir);
		float3 dirnorm = normalize(dir);
		float3 fd = f - dot(f, dirnorm) * dirnorm;
		float discriminant = 4.0f * a * (r2 - dot(fd, fd));

		if (discriminant >= 0.0f)
		{
			float c = dot(f, f) - r2;
			float sqrtVal = sqrt(discriminant);
			SphereAttribs sphrAttr = { center };

			// include Press, William H., Saul A. Teukolsky, William T. Vetterling, and Brian P. Flannery, 
			// "Numerical Recipes in C," Cambridge University Press, 1992.
			float q = (b >= 0) ? -0.5f * (b + sqrtVal) : -0.5f * (b - sqrtVal);

			// we don't bother testing for division by zero
			ReportHit(c / q, 0, sphrAttr);
			// more distant hit - not needed if we know we will intersect with the outside of the sphere
			ReportHit(q / a, 0, sphrAttr);
		}
	}
	else if (method == 5) {
		// get rid of 2.0f and 4.0f and 0.5f factors, etc.
		// Hearn and Baker equation 10-72 for when radius^2 << distance between origin and center
		// Also at https://www.cg.tuwien.ac.at/courses/EinfVisComp/Slides/SS16/EVC-11%20Ray-Tracing%20Slides.pdf
		// In our particular application we don't need to normalize here, as the direction is already normalized (or distance doesn't matter).
		float3 f = orig - center;
		float r2 = radius * radius;
		float a = dot(dir, dir);
		//float b = 2.0f * dot(f, dir);
		float b2 = dot(f, dir);
		float3 dirnorm = normalize(dir);
		float3 fd = f - dot(f, dirnorm) * dirnorm;
		float discriminant = a * (r2 - dot(fd, fd));

		if (discriminant >= 0.0f)
		{
			float c = dot(f, f) - r2;
			float sqrtVal = sqrt(discriminant);
			SphereAttribs sphrAttr = { center };

			// include Press, William H., Saul A. Teukolsky, William T. Vetterling, and Brian P. Flannery, 
			// "Numerical Recipes in C," Cambridge University Press, 1992.
			float q = (b2 >= 0) ? -sqrtVal - b2 : sqrtVal - b2;

			// we don't bother testing for division by zero
			ReportHit(c / q, 0, sphrAttr);
			// more distant hit - not needed if we know we will intersect with the outside of the sphere
			ReportHit(q / a, 0, sphrAttr);
		}
	}
	else if (method == 6) {
		// assumed the direction is already normalized
		// Hearn and Baker equation 10-72 for when radius^2 << distance between origin and center
		// Also at https://www.cg.tuwien.ac.at/courses/EinfVisComp/Slides/SS16/EVC-11%20Ray-Tracing%20Slides.pdf
		// In our particular application we don't need to normalize here, as the direction is already normalized (or distance doesn't matter).
		// If not normalized, this next line should be uncommented.
		// dir = normalize(dir);
		float3 f = orig - center;
		float b2 = dot(f, dir);
		float r2 = radius * radius;
		float3 fd = f - b2 * dir;
		float discriminant = r2 - dot(fd, fd);

		if (discriminant >= 0.0f)
		{
			float c = dot(f, f) - r2;
			float sqrtVal = sqrt(discriminant);
			SphereAttribs sphrAttr = { center };

			// include Press, William H., Saul A. Teukolsky, William T. Vetterling, and Brian P. Flannery, 
			// "Numerical Recipes in C," Cambridge University Press, 1992.
			float q = (b2 >= 0) ? -sqrtVal - b2 : sqrtVal - b2;

			// we don't bother testing for division by zero
			ReportHit(c / q, 0, sphrAttr);
			// more distant hit - not needed if we know we will intersect with the outside of the sphere
			ReportHit(q, 0, sphrAttr);
		}
	}
}
