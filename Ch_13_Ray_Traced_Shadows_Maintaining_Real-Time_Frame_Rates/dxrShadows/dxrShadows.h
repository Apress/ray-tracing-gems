#pragma once
#include "Falcor.h"
#include "FalcorExperimental.h"

#include "shaderCommon.h"

using namespace Falcor;

// Helper struct for fullscreen passes
struct FullScreenPassContext {
	GraphicsVars::SharedPtr pVars;
	FullScreenPass::UniquePtr pPass;
};

// Helper struct for standard rasterization passes
struct RasterizationPassContext {
	GraphicsVars::SharedPtr pVars;
	GraphicsProgram::SharedPtr pProgram;
};

// Helper struct for ray tracing passes
struct RaytracingPassContext {
	RtProgramVars::SharedPtr pVars;
	RtProgram::SharedPtr pProgram;
	bool varsAlreadySetup = false;
};

// Helper struct for shadow mapping passes
struct ShadowMappingPass
{
	bool updateShadowMap = true;
	std::vector<CascadedShadowMaps::SharedPtr> mpCsmTech;
	std::vector<Texture::SharedPtr> mpVisibilityBuffers;
	glm::mat4 camVpAtLastCsmUpdate = glm::mat4();
};

// Helper struct for skybox passes
struct SkyBoxPass
{
	std::shared_ptr<SkyBox> pEffect;
	DepthStencilState::SharedPtr pDS;
	Sampler::SharedPtr pSampler;
};

enum class Filtering {
	Off = 0,
	Gaussian = 1,
	Box = 2,
};

enum class DebugVisualizations {
	None = 0,
	Depth = 1,
	ViewSpacePosition = 2,
	WorldSpacePosition = 3,
	MotionVectors = 5,
	Variation = 6,
	WorldSpaceNormals = 10,
	AdaptiveSamplesCounts = 13,
	FilteringKernelSize = 14
};

enum class ShadowsAlgorithm {
	RayTracing = 1,
	ShadowMapping = 2,
	Off = 3
};

enum class DepthNormalsSourceType {
	DepthPrePass = 1,
	GBufferPass = 2,
	PrimaryRays = 3
};

class DXRShadows : public Renderer
{
public:

	// Falcor callbacks
	void onLoad(SampleCallbacks* pSample, RenderContext* pRenderContext) override;
	void onFrameRender(SampleCallbacks* pSample, RenderContext* pRenderContext, const std::shared_ptr<Fbo>& pTargetFbo) override;
	void onResizeSwapChain(SampleCallbacks* pSample, uint32_t width, uint32_t height) override;
	bool onKeyEvent(SampleCallbacks* pSample, const KeyboardEvent& keyEvent) override;
	bool onMouseEvent(SampleCallbacks* pSample, const MouseEvent& mouseEvent) override;
	void onGuiRender(SampleCallbacks* pSample, Gui* pGui) override;

private:
	
	// Scene data
	RtScene::SharedPtr mpScene = nullptr;
	Camera::SharedPtr mpCamera = nullptr;

	std::vector<ShadowsLightInfo> mSortedLights;

	// Renderers
	RtSceneRenderer::SharedPtr mpRaytracingRenderer = nullptr;
	SceneRenderer::SharedPtr mpRasterizingRenderer = nullptr;

	// Graphics state used for rasterization passes (depth pre-pass, G-buffer pass & lighting stage)
	GraphicsState::SharedPtr mpGraphicsState = nullptr;

	// Rendering states
	DepthStencilState::SharedPtr mpNoDepthDS = nullptr;
	DepthStencilState::SharedPtr mpDepthTestLessEqual = nullptr;
	DepthStencilState::SharedPtr mpDepthTestEqual = nullptr;
	
	BlendState::SharedPtr mpOpaqueBS = nullptr;

	RasterizerState::SharedPtr mpCullRastStateNoCulling = nullptr;
	RasterizerState::SharedPtr mpCullRastStateBackfaceCulling = nullptr;

	// ===============================================================================
	// Motion vectors pass
	// ===============================================================================
	
	FullScreenPassContext mMotionVectorsPass;
	Texture::SharedPtr mpNormalsBuffer;
	Texture::SharedPtr mpMotionAndDepthBuffer;
	
	Fbo::SharedPtr mpMotionVectorsPassFbo;
	float mMaxReprojectionDepthDifference = 0.003f;

	glm::mat4 mPreviousViewProjectionMatrix;

	void motionVectorsPass(RenderContext* pContext, Fbo::SharedPtr pTargetFbo);

	// ===============================================================================
	// Variation buffers filtering pass
	// ===============================================================================

	FullScreenPassContext mVariationFilteringPass;
	Fbo::SharedPtr mpVariationFilteringPass1Fbo;
	Fbo::SharedPtr mpVariationFilteringPass2Fbo;

	void variationFilteringPass(RenderContext* pContext, Fbo::SharedPtr pTargetFbo, int pass);

	// ===============================================================================
	// Shadows filtering (denoising) pass
	// ===============================================================================

	FullScreenPassContext mVisibilityFilteringPass;

	// Filtered visibility buffers suitable for shading
	std::vector<Texture::SharedPtr> mRtOutputFilteredVisibilityBuffer;
	std::vector<Texture::SharedPtr> mRtFilteredVariationBufferEven;
	std::vector<Texture::SharedPtr> mRtFilteredVariationBufferOdd;
	std::vector<Texture::SharedPtr> mRtTempBuffers;
	
	// Variation measure and sample counts cache for each light
	std::vector<Texture::SharedPtr> mOutputVariationAndSampleCountCache;

	float mFilteringDepthRelativeDifferenceEpsilonMin = 0.003f;
	float mFilteringDepthRelativeDifferenceEpsilonMax = 0.02f;
	float mFilteringDotNormalsEpsilon = 0.9f;
	float mFilteringMaxVariationLevel = 0.4f;

	Fbo::SharedPtr mpVisibilityFilteringPass1Fbo;
	Fbo::SharedPtr mpVisibilityFilteringPass2Fbo;
	
	void visibilityFilteringPass(RenderContext* pContext, Fbo::SharedPtr pTargetFbo, int pass);

	// ===============================================================================
	// Debug visualizations pass
	// ===============================================================================
	
	FullScreenPassContext mDebugVisualizationsPass;

	DebugVisualizations mDebugVisualizationType = DebugVisualizations::None;

	void debugVisualizationsPass(RenderContext* pContext, Fbo::SharedPtr pTargetFbo);

	// ===============================================================================
	// Depth pre-pass
	// ===============================================================================

	Fbo::SharedPtr mpEvenDepthPassFbo;
	Fbo::SharedPtr mpOddDepthPassFbo;

	void depthPass(RenderContext* pContext);

	RasterizationPassContext mDepthPass;

	// ===============================================================================
	// G-Buffer pre-pass
	// ===============================================================================

	Fbo::SharedPtr mpEvenGBufferPassFbo;
	Fbo::SharedPtr mpOddGBufferPassFbo;

	void GBufferPass(RenderContext* pContext);

	RasterizationPassContext mGBufferPass;

	// ===============================================================================
	// Shadow mapping pass
	// ===============================================================================

	ShadowMappingPass mShadowPass;

	void shadowMappingPass(RenderContext* pContext);

	// ===============================================================================
	// Ray traced shadows pass
	// ===============================================================================

	std::vector<Texture::SharedPtr> mRtVisibilityCacheOdd;
	std::vector<Texture::SharedPtr> mRtVisibilityCacheEven;
	RtState::SharedPtr mpRtState;

	// Sampling matrix parameters
	bool mEnableSamplingMatrix = true;
	int mSamplingMatrixBlockSize = 8;

	// Shadow sampling controls
	unsigned int mLightSamplesCount = 4;
	bool mEnableAdaptiveSampling = true;
	bool mEnableAdaptiveHighQuality = false;

	// Global sample count limiting parameters
	float mVariationGlobalFalloffSpeed = 2.0f;
	float mVariationGlobalThreshold = 0.3f;
	bool mEnableGlobalSampleCountLimiting = false;

	// Quality parameters
	float mTargetVariation = 0.1f;
	int mMaxRayRecursionDepth = 3;

	RaytracingPassContext mRtShadowsPass;

	void rayTracedShadowsPass(RenderContext* pContext, Fbo::SharedPtr pTargetFbo, int pass);

	// ===============================================================================
	// Lighting pass
	// ===============================================================================
	
	Fbo::SharedPtr mpLightingPassFbo;

	RasterizationPassContext mLightingPass;

	unsigned int mFrameNumber;

	void lightingPass(RenderContext* pContext, Fbo::SharedPtr pTargetFbo);

	void renderScene(RenderContext* pContext, Fbo::SharedPtr pTargetFbo);

	void renderSkyBox(RenderContext* pContext, Fbo::SharedPtr pTargetFbo);

	// ===============================================================================
	// SkyBox pass
	// ===============================================================================

	SkyBoxPass mSkyBox;

	// ===============================================================================
	// Utilities
	// ===============================================================================

	// Samples
	Texture::SharedPtr mpPoisson1to32sppSamples;

	// Input controllers
	FirstPersonCameraController mCamController;

	// Shadows settings
	ShadowsAlgorithm mShadowsAlgorithm = ShadowsAlgorithm::RayTracing;

	// Pipeline settings
	DepthNormalsSourceType mDepthNormalsSource = DepthNormalsSourceType::GBufferPass;

	// Global size override for lights
	float mPointLightSizeOverride = 0.5f;
	float mDirectionalLightSolidAngleOverride = 0.05f;

	// Debug controls
	bool mShowShadowsOnly = true;
	unsigned int mVisibilityBufferFalcorIdToDisplay = INVALID_LIGHT_INDEX; //< Defaults to displaying all lights
	unsigned int mVisibilityBufferToDisplay = INVALID_LIGHT_INDEX; //< Defaults to displaying all lights

	// Utils
	uint32_t swapChainLightCount;
	uint32_t swapChainWidth;
	uint32_t swapChainHeight;
	float mNearPlane = 0.1f;
	float mFarPlane = 100.0f;
	float mCameraSpeed = 0.5f;
	BoundingBox mSceneBox;
	float mMaxRayLength;

	Fbo::SharedPtr getCurrentFrameDepthFBO();
	Fbo::SharedPtr getPreviousFrameDepthFBO();

	void loadScene(const std::string& filename, const Fbo* pTargetFbo);
	void applyDefine(Program::SharedPtr pProgram, bool enabled, std::string define, std::string value);
	void applyDefine(RtProgram::SharedPtr pProgram, bool enabled, std::string define, std::string value);

	int getLightsPerPassNum(int passNum);
	int getPackedTexturesPerPassNum(int passNum);
	int getPackedTexturesPerSceneNum();
	int getPassesPerSceneNum();

	void updateLightBuffer();
	
};
