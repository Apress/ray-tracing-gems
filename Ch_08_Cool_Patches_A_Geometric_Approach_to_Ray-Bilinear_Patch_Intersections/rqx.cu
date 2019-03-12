RT_PROGRAM void intersectPatch(int prim_idx) {
	// ray is rtDeclareVariable(Ray,ray,rtCurrentRay,) in OptiX
	const PatchData& patch = patchdata[prim_idx]; // patchdata is optix::rtBuffer
	const float3* q = patch.coefficients();       // 4 corners + "normal" qn
	float3 q00 = q[0], q10 = q[1], q11 = q[2], q01 = q[3];
	float3 e10 = q10 - q00; // q01---------------q11
	float3 e11 = q11 - q10; // |                   |
	float3 e00 = q01 - q00; // | e00           e11 |  we precompute
	float3 qn  = q[4];      // |        e10        |  qn = cross(q10-q00,q01-q11)
	q00 -= ray.origin;      // q00---------------q10
	q10 -= ray.origin;
	float a = dot(cross(q00, ray.direction), e00); // the equation is
	float c = dot(qn, ray.direction);              // a + b u + c u^2
	float b = dot(cross(q10, ray.direction), e11); // first compute a+b+c
	b -= a + c;                                    // and then b
	float det = b*b - 4*a*c;
	if (det < 0) return;      // see the right part of Figure 8.5
	det = sqrt(det);          // we -use_fast_math in CUDA_NVRTC_OPTIONS
	float u1, u2;             // two roots (u parameter)
	float t = ray.tmax, u, v; // need solution for the smallest t > 0  
	if (c == 0) {                        // if c == 0, it is a trapezoid
		u1  = -a/b; u2 = -1;              // and there is only one root
	} else {                             // (c != 0 in Stanford models)
		u1  = (-b - copysignf(det, b))/2; // numerically "stable" root
		u2  = a/u1;                       // Viete's formula for u1*u2
		u1 /= c;
	}
	if (0 <= u1 && u1 <= 1) {                // is it inside the patch?
		float3 pa = lerp(q00, q10, u1);       // point on edge e10 (Figure 8.4)
		float3 pb = lerp(e00, e11, u1);       // it is, actually, pb - pa
		float3 n  = cross(ray.direction, pb);
		det = dot(n, n);
		n = cross(n, pa);
		float t1 = dot(n, pb);
		float v1 = dot(n, ray.direction);     // no need to check t1 < t		
		if (t1 > 0 && 0 <= v1 && v1 <= det) { // if t1 > ray.tmax, 					
			t = t1/det; u = u1; v = v1/det;    // it will be rejected				
		}                                     // in rtPotentialIntersection
	}
	if (0 <= u2 && u2 <= 1) {                // it is slightly different,
		float3 pa = lerp(q00, q10, u2);       // since u1 might be good
		float3 pb = lerp(e00, e11, u2);       // and we need 0 < t2 < t1
		float3 n  = cross(ray.direction, pb);
		det = dot(n, n);
		n = cross(n, pa);
		float t2 = dot(n, pb)/det;
		float v2 = dot(n, ray.direction);
		if (0 <= v2 && v2 <= det && t > t2 && t2 > 0) {
			t = t2; u = u2; v = v2/det;
		}
	}
	if (rtPotentialIntersection(t)) {
		// Fill the intersection structure irec.
		// Normal(s) for the closest hit will be normalized in a shader.
		float3 du = lerp(e10, q11 - q01, v);
		float3 dv = lerp(e00, e11, u);
		irec.geometric_normal = cross(du, dv);
		#if defined(SHADING_NORMALS)
		const float3* vn = patch.vertex_normals;
		irec.shading_normal = lerp(lerp(vn[0],vn[1],u), 
		                           lerp(vn[3],vn[2],u),v);
		#else
		irec.shading_normal = irec.geometric_normal;
		#endif
		irec.texcoord = make_float3(u, v, 0);
		irec.id = prim_idx;
		rtReportIntersection(0u);
	}
}
