# Importance Sampling of Many Lights on the GPU

The code for the article can be found in the [Falcor][Falcor] repository since
the [Falcor 4.0][Falcor4.0] release.

**Note**: All links further down specifically point to a certain revision (the
latest commit as-of writing) of the files to ensure that those links stay valid
even if the files are moved around in future versions of Falcor.

The management (building, keeping up to date, computing various stats, etc.),
as well as sampling of the light BVH are described in the files starting with
the 'LightBVH' prefix in the
[_Source/Falcor/Experimental/Scene/Lights_][LightsFolder] folder. In
particular, the sampling of the light BVH on the GPU can be found in the
[LightBVHSampler.slang][LightBVHSampler] file.

To add the light BVH sampler to your own render pass, you should create an
[`EmissiveLightSampler`][EmissiveLightSampler] instance of type
`EmissiveLightSamplerType::LightBVH`; you can see how it is used in the
[`PathTracer`][PathTracer] class, and see it in action by loading the
[PathTracer.py][PathTracerScript] script file in Mogwai.

[Falcor]: https://github.com/NVIDIAGameWorks/Falcor
[Falcor4.0]: https://github.com/NVIDIAGameWorks/Falcor/tree/76816c0e5c1d4490d11604bda692800b921d68e3
[LightsFolder]: https://github.com/NVIDIAGameWorks/Falcor/tree/300aee1d7a9609e427f07e8887fd9bcb377426b0/Source/Falcor/Experimental/Scene/Lights
[LightBVHSampler]: https://github.com/NVIDIAGameWorks/Falcor/tree/300aee1d7a9609e427f07e8887fd9bcb377426b0/Source/Falcor/Experimental/Scene/Lights/LightBVHSampler.h
[EmissiveLightSampler]: https://github.com/NVIDIAGameWorks/Falcor//tree/300aee1d7a9609e427f07e8887fd9bcb377426b0/Source/Falcor/Experimental/Scene/Lights/EmissiveLightSampler.h
[PathTracer]: https://github.com/NVIDIAGameWorks/Falcor/blob/300aee1d7a9609e427f07e8887fd9bcb377426b0/Source/Falcor/RenderPasses/Shared/PathTracer/PathTracer.h
[PathTracerScript]: https://github.com/NVIDIAGameWorks/Falcor/blob/300aee1d7a9609e427f07e8887fd9bcb377426b0/Source/Mogwai/Data/PathTracer.py
