/*
 * Copyright (c) 2019 NVIDIA Corporation.  All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property and proprietary
 * rights in and to this software, related documentation and any modifications thereto.
 * Any use, reproduction, disclosure or distribution of this software and related
 * documentation without an express license agreement from NVIDIA Corporation is strictly
 * prohibited.
 *
 * TO THE MAXIMUM EXTENT PERMITTED BY APPLICABLE LAW, THIS SOFTWARE IS PROVIDED *AS IS*
 * AND NVIDIA AND ITS SUPPLIERS DISCLAIM ALL WARRANTIES, EITHER EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL NVIDIA OR ITS SUPPLIERS BE LIABLE FOR ANY
 * SPECIAL, INCIDENTAL, INDIRECT, OR CONSEQUENTIAL DAMAGES WHATSOEVER (INCLUDING, WITHOUT
 * LIMITATION, DAMAGES FOR LOSS OF BUSINESS PROFITS, BUSINESS INTERRUPTION, LOSS OF
 * BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS) ARISING OUT OF THE USE OF OR
 * INABILITY TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS BEEN ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES
 */

// This is a supplementary material for ray/patch intersection from the Chapter 8
// in Ray Tracing Gems book (http://www.realtimerendering.com/raytracinggems).
// It provides C++/CUDA implementation of ray/patch intersector in world and ray-centric coordinates 
// and tests it using the same data as in the accompanied Mathematica notebook.

// We use float3, make_float3, copysignf, dot, cross, lerp, and Ray from Optix
// (https://developer.nvidia.com/optix)
// Any other implementation of these functions will work as well.

#define NOMINMAX
#include <optixu/optixu_math_stream_namespace.h>
using namespace optix;

#include <iostream>

// define IDE debugging stream
#if defined(_MSC_BUILD)
#include <windows.h>
struct ide_stream : std::streambuf {
	int overflow(int s) {
		OutputDebugStringA((LPCSTR)&s); // output character to MS IDE
		int c = putchar(s);             // output character to console
		std::cout << std::flush;        // and make it stick
		return c;
	}
	ide_stream() {
		std::cout.rdbuf(this);          // redirect these streams
		std::cerr.rdbuf(this);          // to Visual Studio IDE Output window
	}
} ide_stream;
#endif

#pragma warning(disable: 4305) // truncation from 'double' to 'float'

void intersectPatchWorldCoordinates(const float3* q, const Ray& ray, float& t, float& u, float& v) {
	// need solution for the smallest t > 0  
	t = std::numeric_limits<float>::infinity(); 
	float3 q00 = q[0], q10 = q[1], q11 = q[2], q01 = q[3];
	float3 e10 = q10 - q00; // q01---------------q11
	float3 e11 = q11 - q10; // |                   |
	float3 e00 = q01 - q00; // | e00           e11 |  we precompute
	float3 qn  = q[4];      // |        e10        |  qn = cross(q10-q00,q01-q11)
	q00 -= ray.origin;      // q00---------------q10
	q10 -= ray.origin;
	float a = dot(cross(q00, ray.direction), e00); // the equation is /*\label{code:a}*/
	float c = dot(qn, ray.direction);              // a + b u + c u^2 /*\label{code:c}*/
	float b = dot(cross(q10, ray.direction), e11); // first compute a+b+c
	b -= a + c;                                    // and then b /*\label{code:b}*/
	float det = b*b - 4*a*c;
	if (det < 0) return;      // see the right part of Figure /*\ref{fig:cases}*/
	det = sqrt(det);          // we -use_fast_math in CUDA_NVRTC_OPTIONS
	float u1, u2;             // two roots (u parameter)
	if (c == 0) {                       // if c == 0, it is a trapezoid /*\label{code:t}*/
		u1  = -a/b; u2 = -1;              // and there is only one root
	} else {                            // (c != 0 in Stanford models)
		u1  = (-b - copysignf(det, b))/2; // numerically "stable" root /*\label{code:u1}*/
		u2  = a/u1;                       // Viete's formula for u1*u2
		u1 /= c;
	}
	if (0 <= u1 && u1 <= 1) {               // is it inside the patch?
		float3 pa = lerp(q00, q10, u1);       // point on edge e10 (Figure /*\ref{fig:algorithm_garq}*/) /*\label{code:v1}*/
		float3 pb = lerp(e00, e11, u1);       // it is, actually, pb - pa
		float3 n  = cross(ray.direction, pb);
		det = dot(n, n);
		n = cross(n, pa);
		float t1 = dot(n, pb);
		float v1 = dot(n, ray.direction);     // no need to check t1 < t		
		if (t1 > 0 && 0 <= v1 && v1 <= det) { // if t1 > ray.tmax, 					
			t = t1/det; u = u1; v = v1/det;     // it will be rejected				
		}                                     // in rtPotentialIntersection
	}
	if (0 <= u2 && u2 <= 1) {               // it is slightly different,
		float3 pa = lerp(q00, q10, u2);       // since u1 might be good /*\label{code:v2}*/
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
}

inline void donb(const float3& raydir, float3& axis1, float3& axis2) {
	// using Duff et al "Building an Orthonormal Basis, Revisited"
	// http://jcgt.org/published/0006/01/01/
	const float3& axis3 = raydir;
	float sign = copysignf(1.0f, axis3.z);
	const float a = -1.0f / (sign + axis3.z);
	const float b = axis3.x * axis3.y * a;
	axis1 = make_float3(1.0f + sign * axis3.x * axis3.x * a, sign * b, -sign * axis3.x);
	axis2 = make_float3(b, sign + axis3.y * axis3.y * a, -axis3.y);
}

// transform four q vectors to axis123 basis
inline void transform(const float3& center, const float3& axis1, const float3& axis2, const float3& axis3, float3* q, int n = 4) {
	for (int i = 0; i < n; i++) {
		float3 cr = q[i] - center;
		q[i].x = dot(cr, axis1);
		q[i].y = dot(cr, axis2);
		q[i].z = dot(cr, axis3);
	}
}

void intersectPatchRayCentricCoordinates(const float3* q, float& t, float& u, float& v) {
	t = std::numeric_limits<float>::infinity(); 

	float a = q[0].y*q[3].x - q[0].x*q[3].y;
	float c = (q[0].y - q[1].y)*(q[3].x - q[2].x) + (q[0].x - q[1].x)*(q[2].y - q[3].y);
	float b = q[1].y*q[2].x - q[1].x*q[2].y;
	b -= a + c;

	float det = b*b - 4*a*c;
	if (det < 0) return;
	det = sqrt(det);

	float u1, u2;

	if (c == 0) {
		u1 = -a/b; u2 = -1;
	} else {
		u1  = (-b - copysignf(det, b))/2;
		u2  = a/u1;
		u1 /= c;
	}

	if (0 <= u1 && u1 <= 1) {
		float3 po =      lerp(q[0], q[1], u1);
		float3 pd = po - lerp(q[3], q[2], u1);
		det       = pd.x*pd.x + pd.y*pd.y;
		float v1  = pd.x*po.x + pd.y*po.y;
		if (0 <= v1 && v1 <= det) {
			v1 /= det;
			float t1 = po.z - v1 * pd.z;
			if (t1 > 0) {
				t = t1; u = u1; v = v1;
			}
		}
	}

	if (0 <= u2 && u2 <= 1) {
		float3 po =      lerp(q[0], q[1], u2);
		float3 pd = po - lerp(q[3], q[2], u2);
		det       = pd.x*pd.x + pd.y*pd.y;
		float v2  = pd.x*po.x + pd.y*po.y;
		if (0 <= v2 && v2 <= det) {
			v2 /= det;
			float t2 = po.z - v2 * pd.z;
			if (t > t2 && t2 > 0) {
				t = t2; u = u2; v = v2;
			}
		}
	}
}


int main() {
	Ray ray;
	// We use the exact u,v, and t for the initialization
	// and then solve for them (to see if the answer is the same).
	ray.direction = normalize(make_float3(-3, 4, -12));
	float3 intersection = make_float3(0.648, 0.368, 0.748);
	ray.origin = intersection - ray.direction;
	float3 q[5];
	q[0] = make_float3(  0,   0,    1);
	q[1] = make_float3(  1,   0,  0.5); 
	q[2] = make_float3(1.2,   1,  0.8);	 
	q[3] = make_float3(  0, 0.8, 0.85); 
	q[4] = cross(q[1] - q[0], q[3] - q[2]);

	float t, u, v;
	intersectPatchWorldCoordinates(q, ray, t, u, v);

	// float3 qu = lerp(q[1] - q[0], q[2] - q[3], v);
	// float3 qv = lerp(q[3] - q[0], q[2] - q[1], u);
	// float3 nn = cross(qv, qu); // surface normal

	std::cout << std::endl;
	std::cout << "world coordinates      : ";
	std::cout << "u = " << u << "; v = " << v << "; t = " << t << "; intersection = " << ray.origin + t * ray.direction << std::endl;

	float3 axis1, axis2;
	donb(ray.direction, axis1, axis2);
	// no need for q[4]
	transform(ray.origin, axis1, axis2, ray.direction, q);

	intersectPatchRayCentricCoordinates(q, t, u, v);

	std::cout << "ray-centric coordinates: ";
	std::cout << "u = " << u << "; v = " << v << "; t = " << t << "; intersection = " << ray.origin + t * ray.direction << std::endl;
	std::cout << std::endl;

}
