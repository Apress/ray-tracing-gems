# Errata for _Ray Tracing Gems_

Corrections for the book [_Ray Tracing Gems_](http://raytracinggems.com). The pages listed are for the released book itself, not for any preprints or other forms of the articles. A corrected copy of the free PDF version of the book can be found linked from [http://raytracinggems.com](http://raytracinggems.com).

## Significant Errors

Page xvii: The passage "photons travel through the is why its introduction" is missing a line between "the" and "is". The whole passage should be "There is no substitute for this capability when the goal is photorealism, where we need to determine the complicated paths along which photons travel through the virtual world. Ray tracing is a fundamental ingredient of realistic rendering, which is why its introduction to the real-time domain was such a significant step for computer graphics." (_v1.2_)

Page 32: The structure "D3D12\_FEATURE\_DATA\_OPTIONS5" should be "D3D12\_FEATURE\_DATA\_D3D12\_OPTIONS5", with a second "D3D12" in there near the end (_v1.2_)

Page 235: The case "a == 0 && b == 0" is not handled and can result in division by 0; above line 3 add "if (b == 0) b = 1;" (_v1.2_)

Page 241: In Equation 11 change "s+1" to "s+2"; the Phong-like PDF doesn't integrate to 1 over the hemisphere if you include the projection cosine (_v1.2_)

Page 242: Line 1 of the code at the top, change "1+s" to "2+s" (_v1.2_)

Page 302: In Equation 1 the first integral is missing "d\omega_i" at the end (_v1.2_)

## Lesser Errors

Page 133: While easy enough to find by searching, the URL for the first reference is http://aggregate.org/MAGIC/ (_v1.2_)

Page 154: "Dupy" to "Dupuy" (_v1.2_)

Page 205: There is a comma before and after in: GPU,”, - remove the second comma (_v1.4 - forthcoming_)

Page 227: "Box-Müller" to "Box-Muller", no umlaut (_v1.2_)

Page 290: "ILMxLabs" to "ILMxLAB" (_v1.2_)

Page 318: For reference [2] the names should read "Heitz, E., Dupuy, J., Hill, S., and Neubelt, D." (_v1.2_)

Page 443: "kernelfootprint" to "kernel footprint" (_v1.2_)

Page 466: Remove editorial note "// Looks good to me, and I added a comma. /Eric" (_v1.3_)

_Thanks to Stephen Hill, Adam Marrs, Daniel Seibert, and Martin Stich for reporting these errors._

Page last updated **March 4, 2019**