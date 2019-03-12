#include "dxrShadows.h"
#include "samplesData.h"

static const glm::vec4 kClearColor(1, 1, 1, 1);
static std::string kDefaultScene = "Arcade/Arcade.fscene";

void DXRShadows::onGuiRender(SampleCallbacks* pSample, Gui* pGui)
{

	if (pGui->addButton("Load Scene"))
	{
		std::string filename;
		if (openFileDialog(Scene::kFileExtensionFilters, filename))
		{
			loadScene(filename, pSample->getCurrentFbo().get());

			// Make sure to initialize FBOs etc.
			onResizeSwapChain(pSample, pSample->getCurrentFbo()->getWidth(), pSample->getCurrentFbo()->getHeight());
		}
	}

	// Shadows algorithm enum
	Gui::DropdownList dlShadowsAlgorithm {
		Gui::DropdownValue{ (int32_t)ShadowsAlgorithm::RayTracing, "Raytraced Shadows" },
		Gui::DropdownValue{ (int32_t)ShadowsAlgorithm::ShadowMapping, "Shadow Mapping" },
		Gui::DropdownValue{ (int32_t)ShadowsAlgorithm::Off, "Off" },
	};

	uint32_t uintShadowsAlgorithm = (uint32_t) mShadowsAlgorithm;

	if (pGui->addDropdown("Shadows Algorithm", dlShadowsAlgorithm, uintShadowsAlgorithm))
	{
		mShadowsAlgorithm = (ShadowsAlgorithm) uintShadowsAlgorithm;
		applyDefine(mLightingPass.pProgram, mShadowsAlgorithm == ShadowsAlgorithm::ShadowMapping, "USE_SHADOW_MAPPING", "");
		applyDefine(mLightingPass.pProgram, mShadowsAlgorithm == ShadowsAlgorithm::RayTracing, "USE_RAYTRACED_SHADOWS", "");
		applyDefine(mLightingPass.pProgram, mShadowsAlgorithm == ShadowsAlgorithm::Off, "SHADOWS_OFF", "");
	}

	// Shadows algorithm enum
	Gui::DropdownList dlDepthSource {
		Gui::DropdownValue{ (int32_t)DepthNormalsSourceType::DepthPrePass, "Depth Pre-Pass" },
		Gui::DropdownValue{ (int32_t)DepthNormalsSourceType::GBufferPass, "G-Buffer Pass" },
		Gui::DropdownValue{ (int32_t)DepthNormalsSourceType::PrimaryRays, "Primary Rays" },
	};

	uint32_t uintDepthSource = (uint32_t)mDepthNormalsSource;

	if (pGui->addDropdown("Depth & Normals Source", dlDepthSource, uintDepthSource))
	{
		mDepthNormalsSource = (DepthNormalsSourceType)uintDepthSource;
		applyDefine(mRtShadowsPass.pProgram, mDepthNormalsSource == DepthNormalsSourceType::PrimaryRays, "USE_PRIMARY_RAYS", "");
		applyDefine(mRtShadowsPass.pProgram, mDepthNormalsSource == DepthNormalsSourceType::GBufferPass, "USE_GBUFFER_DEPTH", "");
	}

	if (pGui->addCheckBox("Display Shadows Only", mShowShadowsOnly))
	{
		applyDefine(mLightingPass.pProgram, mShowShadowsOnly, "DISPLAY_SHADOWS_ONLY", "");
	}

	Gui::DropdownList dlLightMask;

	dlLightMask.push_back(Gui::DropdownValue{ INVALID_LIGHT_INDEX, "ALL" });

	if (mpScene != nullptr) {
		for (unsigned int i = 0; i < mpScene->getLightCount(); i++) {
			auto light = mpScene->getLight(i);

			dlLightMask.push_back(Gui::DropdownValue{ i, light->getName() });
		}
	}

	if (pGui->addDropdown("Lights mask", dlLightMask, mVisibilityBufferFalcorIdToDisplay)) {

		if (mVisibilityBufferFalcorIdToDisplay == INVALID_LIGHT_INDEX) {
			mVisibilityBufferToDisplay = (unsigned int)INVALID_LIGHT_INDEX;
		} else {
			mVisibilityBufferToDisplay = (unsigned int)mVisibilityBufferFalcorIdToDisplay;
		}
	}

	if (pGui->addFloatVar("Directional Light Size Override", mDirectionalLightSolidAngleOverride, 0.0f)) {
		for (uint32_t i = 0; i < mSortedLights.size(); i++)
		{
			if (mSortedLights[i].type == DIRECTIONAL_HARD_LIGHT || mSortedLights[i].type == DIRECTIONAL_SOFT_LIGHT)
				mSortedLights[i].size = mDirectionalLightSolidAngleOverride;
		}
	}

	if (pGui->addFloatVar("Point Light Size Override", mPointLightSizeOverride, 0.0f)) {
		for (uint32_t i = 0; i < mSortedLights.size(); i++)
		{
			if (mSortedLights[i].type == SPHERICAL_LIGHT || mSortedLights[i].type == POINT_LIGHT)
				mSortedLights[i].size = mPointLightSizeOverride;
		}
	}

	if (pGui->beginGroup("Shadows sizes"))
	{
			for (uint32_t i = 0; i < mpScene->getLightCount(); i++)
			{
				float temp = mSortedLights[i].size;
				if (pGui->addFloatVar((mpScene->getLight(i)->getName() + " Size").c_str() , temp, 0.0f)) {
					mSortedLights[i].size = temp;
				}
			}
		pGui->endGroup();
	}

	if (pGui->addCheckBox("Enable Adaptive Sampling", mEnableAdaptiveSampling))
	{
		applyDefine(mRtShadowsPass.pProgram, mEnableAdaptiveSampling, "USE_ADAPTIVE_SAMPLING", "");
		applyDefine(mDebugVisualizationsPass.pPass->getProgram(), mEnableAdaptiveSampling, "USE_ADAPTIVE_SAMPLING", "");
	}

	if (mEnableAdaptiveSampling) {
		Gui::DropdownList dlQuality{
			Gui::DropdownValue{ 0, "Standard Quality" },
			Gui::DropdownValue{ 1, "High Quality" },
		};

		uint32_t uintQuality = mEnableAdaptiveHighQuality ? 1 : 0;

		if (pGui->addDropdown("Quality", dlQuality, uintQuality))
		{
			if (uintQuality == 0)
				mEnableAdaptiveHighQuality = false;
			else
				mEnableAdaptiveHighQuality = true;
		}
	}

	if (!mEnableAdaptiveSampling) {

		if (mLightSamplesCount > 32) {
			mLightSamplesCount = 32;
		}

		int sampleCountInt = (int)mLightSamplesCount;
		if (pGui->addIntVar("Light samples Count", sampleCountInt, 1, 32)) {

			if (sampleCountInt > (int)mLightSamplesCount) {
				// increase
				if (sampleCountInt >= 9 && sampleCountInt <= 11) sampleCountInt = 12;
				if (sampleCountInt >= 13 && sampleCountInt <= 15) sampleCountInt = 16;
				if (sampleCountInt >= 17 && sampleCountInt <= 19) sampleCountInt = 20;
				if (sampleCountInt >= 21 && sampleCountInt <= 23) sampleCountInt = 24;
				if (sampleCountInt >= 25 && sampleCountInt <= 27) sampleCountInt = 28;
				if (sampleCountInt >= 29 && sampleCountInt <= 31) sampleCountInt = 32;
			}
			else {
				// decrease
				if (sampleCountInt >= 9 && sampleCountInt <= 11) sampleCountInt = 8;
				if (sampleCountInt >= 13 && sampleCountInt <= 15) sampleCountInt = 12;
				if (sampleCountInt >= 17 && sampleCountInt <= 19) sampleCountInt = 16;
				if (sampleCountInt >= 21 && sampleCountInt <= 23) sampleCountInt = 20;
				if (sampleCountInt >= 25 && sampleCountInt <= 27) sampleCountInt = 24;
				if (sampleCountInt >= 29 && sampleCountInt <= 31) sampleCountInt = 28;
			}

			mLightSamplesCount = (unsigned int)sampleCountInt;
		}
	}

	if (pGui->addCheckBox("Apply Sampling Matrix", mEnableSamplingMatrix))
	{
		applyDefine(mRtShadowsPass.pProgram, mEnableSamplingMatrix, "APPLY_SAMPLING_MATRIX", "");
	}

	if (pGui->addFloatVar("Near Plane", mNearPlane, 0.0f)) {
		mpCamera->setDepthRange(mNearPlane, mFarPlane);
	}
	if (pGui->addFloatVar("Far Plane", mFarPlane, 0.0f)) {
		mpCamera->setDepthRange(mNearPlane, mFarPlane);
	}
	if (pGui->addFloatVar("Camera Speed", mCameraSpeed, 0.0f)) {
		mCamController.setCameraSpeed(mCameraSpeed);
	}

	// Debug visualizations enum
	Gui::DropdownList dlDebugType {
		Gui::DropdownValue{ (int32_t)DebugVisualizations::None, "None" },
		Gui::DropdownValue{ (int32_t)DebugVisualizations::Depth, "Depth" },
		Gui::DropdownValue{ (int32_t)DebugVisualizations::ViewSpacePosition, "View Space Position" },
		Gui::DropdownValue{ (int32_t)DebugVisualizations::WorldSpacePosition, "World Space Position" },
		Gui::DropdownValue{ (int32_t)DebugVisualizations::MotionVectors, "Motion Vectors" },
		Gui::DropdownValue{ (int32_t)DebugVisualizations::Variation, "Variation Levels" },
		Gui::DropdownValue{ (int32_t)DebugVisualizations::WorldSpaceNormals, "World Space Normals" },
		Gui::DropdownValue{ (int32_t)DebugVisualizations::AdaptiveSamplesCounts, "Sample Counts" },
		Gui::DropdownValue{ (int32_t)DebugVisualizations::FilteringKernelSize, "Filtering Kernel Size" },
	};
	
	uint32_t uintDebugType = (uint32_t) mDebugVisualizationType;

	if (pGui->addDropdown("Debug Output", dlDebugType, uintDebugType))
	{
		mDebugVisualizationType = (DebugVisualizations)uintDebugType;
		applyDefine(mDebugVisualizationsPass.pPass->getProgram(), mDebugVisualizationType == DebugVisualizations::Depth, "OUTPUT_DEPTH", "");
		applyDefine(mDebugVisualizationsPass.pPass->getProgram(), mDebugVisualizationType == DebugVisualizations::ViewSpacePosition, "OUTPUT_VIEW_SPACE_POSITION", "");
		applyDefine(mDebugVisualizationsPass.pPass->getProgram(), mDebugVisualizationType == DebugVisualizations::WorldSpacePosition, "OUTPUT_WORLD_SPACE_POSITION", "");
		applyDefine(mDebugVisualizationsPass.pPass->getProgram(), mDebugVisualizationType == DebugVisualizations::MotionVectors, "OUTPUT_MOTION_VECTORS", "");
		applyDefine(mDebugVisualizationsPass.pPass->getProgram(), mDebugVisualizationType == DebugVisualizations::Variation, "OUTPUT_VARIATION_LEVEL", "");
		applyDefine(mDebugVisualizationsPass.pPass->getProgram(), mDebugVisualizationType == DebugVisualizations::WorldSpaceNormals, "OUTPUT_NORMALS_WORLD", "");
		applyDefine(mDebugVisualizationsPass.pPass->getProgram(), mDebugVisualizationType == DebugVisualizations::AdaptiveSamplesCounts, "OUTPUT_ADAPTIVE_SAMPLES_COUNT", "");
		applyDefine(mDebugVisualizationsPass.pPass->getProgram(), mDebugVisualizationType == DebugVisualizations::FilteringKernelSize, "OUTPUT_FILTERING_KERNEL_SIZE", "");
	}
	
	if (pGui->beginGroup("Shadow Mapping Settings"))
	{
		
		if (mShadowsAlgorithm == ShadowsAlgorithm::ShadowMapping)
		{
			pGui->addCheckBox("Update Map", mShadowPass.updateShadowMap);

			for (uint32_t i = 0; i < mShadowPass.mpCsmTech.size(); i++)
			{
				mShadowPass.mpCsmTech[i]->renderUI(pGui, ("CSM " + std::to_string(i)).c_str());
			}
		}
		pGui->endGroup();
	}

	if (mpScene != nullptr) {
		for (uint32_t i = 0; i < mpScene->getLightCount(); i++)
		{
			mpScene->getLight(i)->renderUI(pGui, mpScene->getLight(i)->getName().c_str());
		}
	}
}

void DXRShadows::loadScene(const std::string& filename, const Fbo* pTargetFbo)
{

	// Load scene
	mpScene = RtScene::loadFromFile(filename, RtBuildFlags::None, Model::LoadFlags::None);

	// Init skybox
	Scene::UserVariable var = mpScene->getUserVariable("sky_box");

	if (var.type == Scene::UserVariable::Type::String) {
		std::string folder = getDirectoryFromFile(filename);
		Sampler::Desc samplerDesc;
		samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
		mSkyBox.pSampler = Sampler::create(samplerDesc);

		mSkyBox.pEffect = SkyBox::create(folder + '/' + var.str, true, mSkyBox.pSampler);

		DepthStencilState::Desc dsDesc;
		dsDesc.setDepthFunc(DepthStencilState::Func::Always);
		mSkyBox.pDS = DepthStencilState::create(dsDesc);
	}
	else {
		mSkyBox.pEffect = nullptr;
	}

	// Remove lights until only 32 remain (at most)
	// This implementation is limited to 32 but can be extended if necessary
	const int maxSupportedLightCount = 32;

	while (mpScene->getLightCount() > maxSupportedLightCount)
	{
		mpScene->deleteLight(maxSupportedLightCount);
	}

	mpCamera = mpScene->getActiveCamera();
	assert(mpCamera);

	mCamController.attachCamera(mpCamera);

	Sampler::Desc samplerDesc;
	samplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Linear);
	Sampler::SharedPtr pSampler = Sampler::create(samplerDesc);

	for (unsigned int i = 0; i < mpScene->getModelCount(); i++) {
		auto m = mpScene->getModel(i);
		m->bindSamplerToMaterials(pSampler);

		mSceneBox = Falcor::BoundingBox::fromUnion(mSceneBox, m->getBoundingBox());
	}

	float radius = glm::length(mSceneBox.extent);
	mMaxRayLength = radius * 10.0f;

	// Update the controllers
	mCamController.setCameraSpeed(radius * 0.25f);
	mNearPlane = std::min(0.1f, std::max(0.01f, radius / 750.0f));
	mFarPlane = radius * 1.5f;
	mCameraSpeed = std::min(2.0f, radius * 0.25f);
	mCamController.setCameraSpeed(mCameraSpeed);
	mpCamera->setDepthRange(mNearPlane, mFarPlane);
	mpCamera->setAspectRatio((float)pTargetFbo->getWidth() / (float)pTargetFbo->getHeight());

	mRtShadowsPass.pVars = RtProgramVars::create(mRtShadowsPass.pProgram, mpScene);

	Sampler::Desc bilinearSamplerDesc;
	bilinearSamplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point);

	mRtShadowsPass.pVars->getRayGenVars()->setSampler("bilinearSampler", Sampler::create(bilinearSamplerDesc));

	mpRaytracingRenderer = RtSceneRenderer::create(mpScene);
	mpRasterizingRenderer = SceneRenderer::create(mpScene);

	mSortedLights.clear();

}

void DXRShadows::onLoad(SampleCallbacks* pSample, RenderContext* pRenderContext)
{

	if (gpDevice->isFeatureSupported(Device::SupportedFeatures::Raytracing) == false)
	{
		logErrorAndExit("Device does not support raytracing!", true);
	}

	// Initialize shadows ray tracing pass
	RtProgram::Desc rtProgDesc;
	rtProgDesc.addShaderLibrary("DXRShadows.rt.hlsl").setRayGen("rayGen");
	rtProgDesc.addHitGroup(0, "primaryClosestHit", "").addMiss(0, "primaryMiss");
	rtProgDesc.addHitGroup(1, "", "shadowAnyHit").addMiss(1, "shadowMiss");

	// Initialize RT shadows pass
	mRtShadowsPass.pProgram = RtProgram::create(rtProgDesc);

	// Initialize lighting pass
	mLightingPass.pProgram = GraphicsProgram::createFromFile("ForwardRenderer.ps.hlsl", "vs", "ps");

	mLightingPass.pVars = GraphicsVars::create(mLightingPass.pProgram->getReflector());
	mpGraphicsState = GraphicsState::create();

	loadScene(kDefaultScene, pSample->getCurrentFbo().get());

	mpRtState = RtState::create();
	mpRtState->setProgram(mRtShadowsPass.pProgram);

	mpRtState->setMaxTraceRecursionDepth(mMaxRayRecursionDepth);

	// Prepare buffer for poisson samples
	mpPoisson1to32sppSamples = Texture::create1D(6048, ResourceFormat::RG32Float, 1, 1, getPoissonDisk1to32spp(), Resource::BindFlags::UnorderedAccess | Resource::BindFlags::ShaderResource);

	// Initialize depth pre-pass
	mDepthPass.pProgram = GraphicsProgram::createFromFile("DepthPass.ps.hlsl", "", "main");
	mDepthPass.pVars = GraphicsVars::create(mDepthPass.pProgram->getReflector());

	// Initialize G-Buffer pass
	mGBufferPass.pProgram = GraphicsProgram::createFromFile("DepthAndNormalsPass.ps.hlsl", "", "main");
	mGBufferPass.pVars = GraphicsVars::create(mGBufferPass.pProgram->getReflector());
	Sampler::Desc bilinearSamplerDesc;
	bilinearSamplerDesc.setFilterMode(Sampler::Filter::Linear, Sampler::Filter::Linear, Sampler::Filter::Point);
	mGBufferPass.pVars->setSampler("bilinearSampler", Sampler::create(bilinearSamplerDesc));

	// Create rasterizer states
	DepthStencilState::Desc dsDesc;
	dsDesc.setDepthTest(false);
	mpNoDepthDS = DepthStencilState::create(dsDesc);
	dsDesc.setDepthTest(true).setStencilTest(false).setDepthFunc(DepthStencilState::Func::LessEqual);
	mpDepthTestLessEqual = DepthStencilState::create(dsDesc);

	dsDesc.setDepthTest(true).setStencilTest(false).setDepthFunc(DepthStencilState::Func::Equal);
	mpDepthTestEqual = DepthStencilState::create(dsDesc);

	BlendState::Desc blendDesc;
	mpOpaqueBS = BlendState::create(blendDesc);

	RasterizerState::Desc rsDesc;
	mpCullRastStateNoCulling = RasterizerState::create(rsDesc);
	rsDesc.setCullMode(RasterizerState::CullMode::Back);
	mpCullRastStateBackfaceCulling = RasterizerState::create(rsDesc);

	// Motion vectors pass (full screen quad)
	mMotionVectorsPass.pPass = FullScreenPass::create("MotionVectors.ps.hlsl");
	mMotionVectorsPass.pVars = GraphicsVars::create(mMotionVectorsPass.pPass->getProgram()->getReflector());
	mMotionVectorsPass.pVars->setSampler("bilinearSampler", Sampler::create(bilinearSamplerDesc));

	// Filtering pass (full screen quad)
	mVisibilityFilteringPass.pPass = FullScreenPass::create("VisibilityFilteringPass.ps.hlsl");
	mVisibilityFilteringPass.pVars = GraphicsVars::create(mVisibilityFilteringPass.pPass->getProgram()->getReflector());
	
	// Variation filtering pass (full screen quad)
	mVariationFilteringPass.pPass = FullScreenPass::create("VariationFilteringPass.ps.hlsl");
	mVariationFilteringPass.pVars = GraphicsVars::create(mVariationFilteringPass.pPass->getProgram()->getReflector());

	// Debug visualizations pass (full screen quad)
	mDebugVisualizationsPass.pPass = FullScreenPass::create("DebugPass.ps.hlsl");
	mDebugVisualizationsPass.pVars = GraphicsVars::create(mDebugVisualizationsPass.pPass->getProgram()->getReflector());

	// Initialize defines
	applyDefine(mLightingPass.pProgram, mShadowsAlgorithm == ShadowsAlgorithm::ShadowMapping, "USE_SHADOW_MAPPING", "");
	applyDefine(mLightingPass.pProgram, mShadowsAlgorithm == ShadowsAlgorithm::RayTracing, "USE_RAYTRACED_SHADOWS", "");
	applyDefine(mLightingPass.pProgram, mShowShadowsOnly, "DISPLAY_SHADOWS_ONLY", "");
	applyDefine(mRtShadowsPass.pProgram, mEnableAdaptiveSampling, "USE_ADAPTIVE_SAMPLING", "");
	applyDefine(mDebugVisualizationsPass.pPass->getProgram(), mEnableAdaptiveSampling, "USE_ADAPTIVE_SAMPLING", "");
	applyDefine(mRtShadowsPass.pProgram, mEnableSamplingMatrix, "APPLY_SAMPLING_MATRIX", "");
	applyDefine(mRtShadowsPass.pProgram, mDepthNormalsSource == DepthNormalsSourceType::PrimaryRays, "USE_PRIMARY_RAYS", "");
	applyDefine(mRtShadowsPass.pProgram, mDepthNormalsSource == DepthNormalsSourceType::GBufferPass, "USE_GBUFFER_DEPTH", "");

	// Initialize matrices
	mPreviousViewProjectionMatrix = mpCamera->getViewProjMatrix();

	// Make sure to initialize FBOs etc.
	onResizeSwapChain(pSample, pSample->getCurrentFbo()->getWidth(), pSample->getCurrentFbo()->getHeight());

}

void DXRShadows::GBufferPass(RenderContext* pContext)
{
	PROFILE("GBufferPass");
	pContext->clearFbo(getCurrentFrameDepthFBO().get(), kClearColor, 1.0f, 0, FboAttachmentType::All);

	mpGraphicsState->setRasterizerState(mpCullRastStateBackfaceCulling);
	mpGraphicsState->setDepthStencilState(mpDepthTestLessEqual);

	// Set inputs
	ConstantBuffer::SharedPtr pCB = mGBufferPass.pVars["GBufferPassCB"];
	pCB["farOverNear"] = mpCamera->getFarPlane() / mpCamera->getNearPlane();

	pCB["normalMatrix"] = glm::transpose(glm::inverse(mpCamera->getViewMatrix()));
	
	pCB["maxReprojectionDepthDifference"] = mMaxReprojectionDepthDifference;
	
	mGBufferPass.pVars->setTexture("previousDepthBuffer", getPreviousFrameDepthFBO()->getDepthStencilTexture());

	mpGraphicsState->setFbo(getCurrentFrameDepthFBO());
	mpGraphicsState->setProgram(mGBufferPass.pProgram);
	pContext->setGraphicsVars(mGBufferPass.pVars);

	mpRasterizingRenderer->renderScene(pContext);
}

void DXRShadows::depthPass(RenderContext* pContext)
{
	PROFILE("depthPass");
	pContext->clearFbo(getCurrentFrameDepthFBO().get(), kClearColor, 1.0f, 0, FboAttachmentType::All);

	mpGraphicsState->setRasterizerState(mpCullRastStateBackfaceCulling);
	mpGraphicsState->setDepthStencilState(mpDepthTestLessEqual);

	mpGraphicsState->setFbo(getCurrentFrameDepthFBO());
	mpGraphicsState->setProgram(mDepthPass.pProgram);
	pContext->setGraphicsVars(mDepthPass.pVars);

	mpRasterizingRenderer->renderScene(pContext);
}

void DXRShadows::motionVectorsPass(RenderContext* pContext, Fbo::SharedPtr pTargetFbo)
{
	PROFILE("motionVectorsPass");

	mpGraphicsState->setFbo(mpMotionVectorsPassFbo);

	// Reset render state
	mpGraphicsState->setRasterizerState(mpCullRastStateNoCulling);
	mpGraphicsState->setBlendState(mpOpaqueBS);
	mpGraphicsState->setDepthStencilState(mpNoDepthDS);

	// Set inputs
	ConstantBuffer::SharedPtr pCB = mMotionVectorsPass.pVars["MotionVectorsPassCB"];
	pCB["farOverNear"] = mpCamera->getFarPlane() / mpCamera->getNearPlane();

	auto projMatrixInv = glm::inverse(mpCamera->getProjMatrix());
	pCB["screenUnprojection"] = glm::float4(projMatrixInv[0][0], projMatrixInv[1][1], projMatrixInv[3][2], 0.0f);
	pCB["screenUnprojection2"] = glm::float4(projMatrixInv[3][0], projMatrixInv[3][1], 0.0f, 1.0f / (projMatrixInv[2][3] + projMatrixInv[3][3]));

	pCB["motionVectorsMatrix"] = mPreviousViewProjectionMatrix * glm::inverse(mpCamera->getViewMatrix());
	
	pCB["texelSizeRcp"] = vec2(1.0f / pTargetFbo->getWidth(), 1.0f / pTargetFbo->getHeight());
	pCB["maxReprojectionDepthDifference"] = mMaxReprojectionDepthDifference;
	
	mMotionVectorsPass.pVars->setTexture("currentDepthBuffer", getCurrentFrameDepthFBO()->getDepthStencilTexture());
	mMotionVectorsPass.pVars->setTexture("previousDepthBuffer", getPreviousFrameDepthFBO()->getDepthStencilTexture());

	// Kick it off
	pContext->setGraphicsVars(mMotionVectorsPass.pVars);
	mMotionVectorsPass.pPass->execute(pContext);
}

void DXRShadows::shadowMappingPass(RenderContext* pContext)
{
	PROFILE("shadowMappingPass");

	// Initialize shadow mapping if needed
	unsigned int lightCount = mpScene->getLightCount();

	if (lightCount > mShadowPass.mpCsmTech.size()) {
		mShadowPass.mpCsmTech.resize(lightCount);
		mShadowPass.mpVisibilityBuffers.resize(lightCount);
		for (uint32_t i = 0; i < lightCount; i++)
		{
			mShadowPass.mpCsmTech[i] = CascadedShadowMaps::create(mpScene->getLight(i), 2048, 2048, swapChainWidth, swapChainHeight, mpScene, 4);
			mShadowPass.mpCsmTech[i]->setFilterMode(CsmFilterEvsm4);
			mShadowPass.mpCsmTech[i]->setVsmLightBleedReduction(0.3f);
			mShadowPass.mpCsmTech[i]->setVsmMaxAnisotropy(4);
			mShadowPass.mpCsmTech[i]->setEvsmBlur(7, 3);
			mShadowPass.mpCsmTech[i]->setDepthBias(0.001f);
		}
	}


	if (mShadowPass.updateShadowMap)
	{
		mShadowPass.camVpAtLastCsmUpdate = mpRaytracingRenderer->getScene()->getActiveCamera()->getViewProjMatrix();
		Texture::SharedPtr pDepth = getCurrentFrameDepthFBO()->getDepthStencilTexture();

		for (uint32_t i = 0; i < mShadowPass.mpCsmTech.size(); i++)
		{
			mShadowPass.mpVisibilityBuffers[i] = mShadowPass.mpCsmTech[i]->generateVisibilityBuffer(pContext, mpRaytracingRenderer->getScene()->getActiveCamera().get(), pDepth);
		}

		pContext->flush();
	}
}

void DXRShadows::updateLightBuffer() {

	if (mSortedLights.size() != mpScene->getLightCount())
		mSortedLights.clear();

	if (mSortedLights.empty()) {

		for (unsigned int i = 0; i < mpScene->getLightCount(); i++) {
			auto l = mpScene->getLight(i);
			ShadowsLightInfo lightInfo;

			switch (l->getType()) {
			case 0:
				lightInfo.type = SPHERICAL_LIGHT;
				lightInfo.size = mPointLightSizeOverride;
				lightInfo.position = l->getData().posW;
				break;
			case 1:
				lightInfo.type = DIRECTIONAL_SOFT_LIGHT;
				lightInfo.size = mDirectionalLightSolidAngleOverride;
				lightInfo.direction = l->getData().dirW;
				break;
			}

			mSortedLights.push_back(lightInfo);
		}

	}
	
	// Update types and positions
	for (uint32_t i = 0; i < uint32_t(mSortedLights.size()); i++) {
		auto l = &(mSortedLights[i]);

		if (l->size > 0.0f) {
			if (l->type == POINT_LIGHT) l->type = SPHERICAL_LIGHT;
			if (l->type == DIRECTIONAL_HARD_LIGHT) l->type = DIRECTIONAL_SOFT_LIGHT;
		} else {
			if (l->type == SPHERICAL_LIGHT) l->type = POINT_LIGHT;
			if (l->type == DIRECTIONAL_SOFT_LIGHT) l->type = DIRECTIONAL_HARD_LIGHT;
		}

		auto falcorLightPosition = mpScene->getLight(i)->getData().posW;
		if (falcorLightPosition != l->position) {
			l->position = falcorLightPosition;
		}

		auto falcorLightDirection = mpScene->getLight(i)->getData().dirW;
		if (falcorLightDirection != l->direction) {
			l->direction = falcorLightDirection;
		}
	}

}

void DXRShadows::rayTracedShadowsPass(RenderContext* pContext, Fbo::SharedPtr pTargetFbo, int pass)
{
	PROFILE("rayTracedShadowsPass");

	GraphicsVars* pVars = mRtShadowsPass.pVars->getGlobalVars().get();
	ConstantBuffer::SharedPtr pCB = pVars->getConstantBuffer("PerFrameCB");

	pCB["invView"] = glm::inverse(mpCamera->getViewMatrix());
	pCB["view"] = mpCamera->getViewMatrix();
	pCB["viewportDims"] = vec2(pTargetFbo->getWidth(), pTargetFbo->getHeight());
	pCB["texelSizeRcp"] = vec2(1.0f / pTargetFbo->getWidth(), 1.0f / pTargetFbo->getHeight());

	float fovY = focalLengthToFovY(mpCamera->getFocalLength(), Camera::kDefaultFrameHeight);
	pCB["tanHalfFovY"] = tanf(fovY * 0.5f);

	auto projMatrixInv = glm::inverse(mpCamera->getProjMatrix());
	pCB["screenUnprojection"] = glm::float4(projMatrixInv[0][0], projMatrixInv[1][1], projMatrixInv[3][2], 0.0f);
	pCB["screenUnprojection2"] = glm::float4(projMatrixInv[3][0], projMatrixInv[3][1], 0.0f, 1.0f / (projMatrixInv[2][3] + projMatrixInv[3][3]));

	pCB["lightSamplesCount"] = (int)mLightSamplesCount;
	pCB["maxLightSamplesCount"] = mEnableAdaptiveHighQuality ? 8 : 5;
	
	pCB["maxRayLength"] = mMaxRayLength;
	pCB["maxRayRecursionDepth"] = mMaxRayRecursionDepth;
	pCB["variationThreshold"] = mTargetVariation;
	pCB["variationBufferBottomMipLevel"] = int(glm::floor(glm::log2(float(glm::max(pTargetFbo->getWidth(), pTargetFbo->getHeight())))));
	pCB["cullingBlockSize"] = (unsigned int) mSamplingMatrixBlockSize;
	
	pCB["inputFilteredVariationGlobalFalloffSpeed"] = mVariationGlobalFalloffSpeed;
	pCB["inputFilteredVariationGlobalThreshold"] = mVariationGlobalThreshold;

	int lightsPerPass = getLightsPerPassNum(pass);
	mRtShadowsPass.pProgram->getRayGenProgram()->addDefine("_LIGHT_COUNT", std::to_string(lightsPerPass));
	int packedLightsPerPass = getPackedTexturesPerPassNum(pass);
	
	for (int i = 0; i < lightsPerPass; i++) {
		auto l = &mSortedLights[(pass * 8) + i];

		std::string lName = "lightsInfo[" + std::to_string(i) + "]";
		pCB[lName + ".type"] = l->type;
		pCB[lName + ".position"] = l->position;
		pCB[lName + ".direction"] = l->direction;
		pCB[lName + ".size"] = l->size;

	}

	// Setup history buffer mask 
	int historyMaskId = mFrameNumber % 4;
	pCB["frameNumber"] = (unsigned int) historyMaskId;

	// Setup input buffers - depth, normals, samples etc.
	mRtShadowsPass.pVars->getRayGenVars()->setTexture("samples", mpPoisson1to32sppSamples);
	mRtShadowsPass.pVars->getRayGenVars()->setTexture("motionAndDepthBuffer", mpMotionAndDepthBuffer);

	std::vector<Texture::SharedPtr>* mpRtInputVisibilityCache;
	std::vector<Texture::SharedPtr>* mpRtOutputVisibilityCache;
	std::vector<Texture::SharedPtr>* mpRtInputFilteredVariationBuffer;
	std::vector<Texture::SharedPtr>* mpRtOutputFilteredVariationBuffer;

	if (mFrameNumber % 2 == 0)
	{
		mpRtInputVisibilityCache = &mRtVisibilityCacheEven;
		mpRtOutputVisibilityCache = &mRtVisibilityCacheOdd;
		mpRtInputFilteredVariationBuffer = &mRtFilteredVariationBufferEven;
		mpRtOutputFilteredVariationBuffer = &mRtFilteredVariationBufferOdd;
	} else {
		mpRtInputVisibilityCache = &mRtVisibilityCacheOdd;
		mpRtOutputVisibilityCache = &mRtVisibilityCacheEven;
		mpRtInputFilteredVariationBuffer = &mRtFilteredVariationBufferOdd;
		mpRtOutputFilteredVariationBuffer = &mRtFilteredVariationBufferEven;
	}

	for (int i = 0; i < lightsPerPass; i++)
	{
		mRtShadowsPass.pVars->getRayGenVars()->setTexture("inputVisibilityCache[" + std::to_string(i) + "]", (*mpRtInputVisibilityCache)[i + (pass * 8)]);
	}
	
	// Setup rt target buffers (per-light visibility buffers)
	for (int i = 0; i < lightsPerPass; i++) {
		mRtShadowsPass.pVars->getRayGenVars()->setUav(0, 0, i, (*mpRtOutputVisibilityCache)[i + (pass * 8)]->getUAV(0, 0, 1));
		mRtShadowsPass.pVars->getRayGenVars()->setUav(0, 8, i, mOutputVariationAndSampleCountCache[i + (pass * 8)]->getUAV(0, 0, 1));
	}
	
	for (int i = 0; i < packedLightsPerPass; i++)
	{
		mRtShadowsPass.pVars->getRayGenVars()->setUav(0, 16, i, mRtOutputFilteredVisibilityBuffer[i + (pass * 2)]->getUAV(0, 0, 1));
		mRtShadowsPass.pVars->getRayGenVars()->setUav(0, 18, i, (*mpRtOutputFilteredVariationBuffer)[i + (pass * 2)]->getUAV(0, 0, 1));
		mRtShadowsPass.pVars->getRayGenVars()->setTexture("inputFilteredVariationBuffer[" + std::to_string(i) + "]", (*mpRtInputFilteredVariationBuffer)[i + (pass * 2)]);
	}
	
	if (mRtVisibilityCacheOdd.size() > 0) {

		// NOTE: this is an optimization that prevents setting up Falcor raytracing variables in each frame.
		// 'mpRaytracingRenderer->renderScene' is only called once upon startup and faster 'pContext->raytrace' method is used since then
		// Should there be problems with dynamic scenes, this optimization should be turned off

		if (!mRtShadowsPass.varsAlreadySetup) {
			mpRaytracingRenderer->renderScene(pContext, mRtShadowsPass.pVars, mpRtState, uvec3(pTargetFbo->getWidth(), pTargetFbo->getHeight(), 1), mpCamera.get());
			mRtShadowsPass.varsAlreadySetup = true;
		}
		else {
			if (!mRtShadowsPass.pVars->apply(pContext, mpRtState->getRtso().get()))
			{
				logError("applying RtProgramVars failed, most likely because we ran out of descriptors.", true);
				assert(false);
			}
			pContext->raytrace(mRtShadowsPass.pVars, mpRtState, pTargetFbo->getWidth(), pTargetFbo->getHeight(), 1);
		}
	}

	// Filter nosie buffers
	mVariationFilteringPass.pPass->getProgram()->addDefine("OPERATOR", std::to_string(MAXIMUM));
	variationFilteringPass(pContext, pTargetFbo, pass);
	mVariationFilteringPass.pPass->getProgram()->addDefine("OPERATOR", std::to_string(AVERAGE));
	variationFilteringPass(pContext, pTargetFbo, pass);

	for (int i = 0; i < packedLightsPerPass; i++) {
		mRtFilteredVariationBufferOdd[i + (pass * 2)]->generateMips(pContext);
		mRtFilteredVariationBufferEven[i + (pass * 2)]->generateMips(pContext);
	}
}

void DXRShadows::variationFilteringPass(RenderContext* pContext, Fbo::SharedPtr pTargetFbo, int pass)
{
	PROFILE("variationFilteringPass");

	// Reset render state
	mpGraphicsState->setRasterizerState(mpCullRastStateNoCulling);
	mpGraphicsState->setBlendState(mpOpaqueBS);
	mpGraphicsState->setDepthStencilState(mpNoDepthDS);

	int packedTexturesCount = getPackedTexturesPerPassNum(pass);
	int lightsPerPassCount = getLightsPerPassNum(pass);

	mVariationFilteringPass.pPass->getProgram()->addDefine("_LIGHT_COUNT_PASS_0", std::to_string(min(lightsPerPassCount, 4)));
	mVariationFilteringPass.pPass->getProgram()->addDefine("_LIGHT_COUNT_PASS_1", std::to_string(max(lightsPerPassCount - 4, 0)));

	for (int i = 0; i < packedTexturesCount; i++) {
		if (mFrameNumber % 2 == 0)
			mVariationFilteringPass.pVars->setTexture("inputBuffer[" + std::to_string(i) + "]", mRtFilteredVariationBufferOdd[i + (pass * 2)]);
		else
			mVariationFilteringPass.pVars->setTexture("inputBuffer[" + std::to_string(i) + "]", mRtFilteredVariationBufferEven[i + (pass * 2)]);
	}
	
	// Kick it off - pass 1 (horizontal)
	pContext->setGraphicsVars(mVariationFilteringPass.pVars);

	mVariationFilteringPass.pPass->getProgram()->addDefine("BLUR_PASS_DIRECTION", std::to_string(BLUR_HORIZONTAL));
	mpGraphicsState->setFbo(mpVariationFilteringPass1Fbo);
	mVariationFilteringPass.pPass->execute(pContext);

	// Kick it off - pass 2 (vertical)
	for (int i = 0; i < packedTexturesCount; i++) {
		if (mFrameNumber % 2 == 0)
			mpVariationFilteringPass2Fbo->attachColorTarget(mRtFilteredVariationBufferOdd[i + (pass * 2)], i);
		else
			mpVariationFilteringPass2Fbo->attachColorTarget(mRtFilteredVariationBufferEven[i + (pass * 2)], i);
	}

	if (packedTexturesCount == 1 && mRtOutputFilteredVisibilityBuffer.size() > 0) {
		// Bind spare buffer to unused slot to prevent runtime DirectX warnings
		mpVariationFilteringPass2Fbo->attachColorTarget(mRtFilteredVariationBufferOdd[1 + (pass * 2)], 1);
	}

	mpGraphicsState->setFbo(mpVariationFilteringPass2Fbo);
	for (int i = 0; i < packedTexturesCount; i++)
	{
		mVariationFilteringPass.pVars->setTexture("inputBuffer[" + std::to_string(i) + "]", mRtTempBuffers[i]);
	}

	mVariationFilteringPass.pPass->getProgram()->addDefine("BLUR_PASS_DIRECTION", std::to_string(BLUR_VERTICAL));
	mVariationFilteringPass.pPass->execute(pContext);

}

void DXRShadows::visibilityFilteringPass(RenderContext* pContext, Fbo::SharedPtr pTargetFbo, int pass)
{
	PROFILE("filteringPass");

	// Reset render state
	mpGraphicsState->setRasterizerState(mpCullRastStateNoCulling);
	mpGraphicsState->setBlendState(mpOpaqueBS);
	mpGraphicsState->setDepthStencilState(mpNoDepthDS);

	// Set inputs
	ConstantBuffer::SharedPtr pCB = mVisibilityFilteringPass.pVars["FilteringPassCB"];
	pCB["depthRelativeDifferenceEpsilonMin"] = mFilteringDepthRelativeDifferenceEpsilonMin;
	pCB["depthRelativeDifferenceEpsilonMax"] = mFilteringDepthRelativeDifferenceEpsilonMax;
	pCB["dotNormalsEpsilon"] = mFilteringDotNormalsEpsilon;
	pCB["farOverNear"] = mpCamera->getFarPlane() / mpCamera->getNearPlane();
	pCB["maxVariationLevel"] = mFilteringMaxVariationLevel;

	mVisibilityFilteringPass.pVars->setTexture("depthBuffer", getCurrentFrameDepthFBO()->getDepthStencilTexture());
	mVisibilityFilteringPass.pVars->setTexture("normalsBuffer", mpNormalsBuffer);

	int packedTexturesCount = getPackedTexturesPerPassNum(pass);
	int lightsPerPassCount = getLightsPerPassNum(pass);

	mVisibilityFilteringPass.pPass->getProgram()->addDefine("_LIGHT_COUNT_PASS_0", std::to_string(min(lightsPerPassCount, 4)));
	mVisibilityFilteringPass.pPass->getProgram()->addDefine("_LIGHT_COUNT_PASS_1", std::to_string(max(lightsPerPassCount - 4, 0)));

	for (int i = 0; i < packedTexturesCount; i++) {
		mVisibilityFilteringPass.pVars->setTexture("inputBuffer[" + std::to_string(i) + "]", mRtOutputFilteredVisibilityBuffer[i + (pass * 2)]);

		if (mFrameNumber % 2 == 0)
			mVisibilityFilteringPass.pVars->setTexture("variationBuffer[" + std::to_string(i) + "]", mRtFilteredVariationBufferOdd[i + (pass * 2)]);
		else
			mVisibilityFilteringPass.pVars->setTexture("variationBuffer[" + std::to_string(i) + "]", mRtFilteredVariationBufferEven[i + (pass * 2)]);

		mpVisibilityFilteringPass2Fbo->attachColorTarget(mRtOutputFilteredVisibilityBuffer[i + (pass * 2)], i);
	}

	if (packedTexturesCount == 1 && mRtOutputFilteredVisibilityBuffer.size() > 0) {
		// Bind spare buffer to unused slot to prevent runtime DirectX warnings
		mpVisibilityFilteringPass1Fbo->attachColorTarget(mRtOutputFilteredVisibilityBuffer[1 + (pass * 2)], 1);
		mpVisibilityFilteringPass2Fbo->attachColorTarget(mRtOutputFilteredVisibilityBuffer[1 + (pass * 2)], 1);
	}

	// Kick it off - pass 1 (horizontal)
	pContext->setGraphicsVars(mVisibilityFilteringPass.pVars);

	mVisibilityFilteringPass.pPass->getProgram()->addDefine("BLUR_PASS_DIRECTION", std::to_string(BLUR_HORIZONTAL));
	mpGraphicsState->setFbo(mpVisibilityFilteringPass1Fbo);
	mVisibilityFilteringPass.pPass->execute(pContext);

	// Kick it off - pass 2 (vertical)
	mpGraphicsState->setFbo(mpVisibilityFilteringPass2Fbo);
	for (int i = 0; i < packedTexturesCount; i++)
	{
		mVisibilityFilteringPass.pVars->setTexture("inputBuffer[" + std::to_string(i) + "]", mRtTempBuffers[i]);
	}

	mVisibilityFilteringPass.pPass->getProgram()->addDefine("BLUR_PASS_DIRECTION", std::to_string(BLUR_VERTICAL));
	mVisibilityFilteringPass.pPass->execute(pContext);
}

void DXRShadows::renderSkyBox(RenderContext* pContext, Fbo::SharedPtr pTargetFbo)
{
	if (mSkyBox.pEffect != nullptr && !mShowShadowsOnly)
	{
		PROFILE("skyBox");
		mpGraphicsState->setFbo(pTargetFbo);

		mpGraphicsState->setDepthStencilState(mSkyBox.pDS);
		mSkyBox.pEffect->render(pContext, mpRasterizingRenderer->getScene()->getActiveCamera().get());
		mpGraphicsState->setDepthStencilState(nullptr);
	}
}

void DXRShadows::lightingPass(RenderContext* pContext, Fbo::SharedPtr pTargetFbo)
{
	PROFILE("lightingPass");

	mpLightingPassFbo->attachDepthStencilTarget(getCurrentFrameDepthFBO()->getDepthStencilTexture());
	mpLightingPassFbo->attachColorTarget(pTargetFbo->getColorTexture(0), 0);

	mpGraphicsState->setFbo(mpLightingPassFbo);
	mpGraphicsState->setProgram(mLightingPass.pProgram);

	mpGraphicsState->setDepthStencilState(mpDepthTestEqual); //< Test for depth equality, our depth buffer is pre-filled
	mpGraphicsState->setBlendState(mpOpaqueBS);
	mpGraphicsState->setRasterizerState(mpCullRastStateBackfaceCulling);

	pContext->setGraphicsVars(mLightingPass.pVars);

	ConstantBuffer::SharedPtr pCB = mLightingPass.pVars->getConstantBuffer("PerFrameCB");

	pCB["visibilityBufferToDisplay"] = (int) mVisibilityBufferToDisplay;
	pCB["camVpAtLastCsmUpdate"] = mShadowPass.camVpAtLastCsmUpdate;

	for (size_t i = 0; i < mSortedLights.size(); i++) {
		auto l = mpScene->getLight(uint32_t(i))->getData();

		std::string n = "lights[" + std::to_string(i) + "]";
		pCB[n + ".posW"] = l.posW;
		pCB[n + ".type"] = l.type;
		pCB[n + ".dirW"] = l.dirW;
		pCB[n + ".openingAngle"] = l.openingAngle;
		pCB[n + ".intensity"] = l.intensity;
		pCB[n + ".cosOpeningAngle"] = l.cosOpeningAngle;
		pCB[n + ".penumbraAngle"] = l.penumbraAngle;
	}

	std::vector<Texture::SharedPtr>* visibilityBuffersToUse = nullptr;
	
	int visibilityBuffersCount = 0;

	if (mShadowsAlgorithm == ShadowsAlgorithm::ShadowMapping)
	{
		visibilityBuffersToUse = &mShadowPass.mpVisibilityBuffers;
		visibilityBuffersCount = mpScene->getLightCount();
	} else {
		visibilityBuffersToUse = &mRtOutputFilteredVisibilityBuffer;
		visibilityBuffersCount = getPackedTexturesPerSceneNum();
	}

	for (int i = 0; i < visibilityBuffersCount; i++)
	{
		mLightingPass.pVars->setTexture("gVisibilityBuffers[" + std::to_string(i) + "]", (*visibilityBuffersToUse)[i]);
	}

	mLightingPass.pProgram->addDefine("_LIGHT_COUNT", std::to_string(mpScene->getLightCount()));

	mpRasterizingRenderer->renderScene(pContext, mpCamera.get());
}

void DXRShadows::debugVisualizationsPass(RenderContext* pContext, Fbo::SharedPtr pTargetFbo)
{
	PROFILE("debugVisualizationsPass");

	if (mDebugVisualizationType == DebugVisualizations::None) return;

	mpGraphicsState->setFbo(pTargetFbo);

	// Reset render state
	mpGraphicsState->setRasterizerState(mpCullRastStateNoCulling);
	mpGraphicsState->setBlendState(mpOpaqueBS);
	mpGraphicsState->setDepthStencilState(mpNoDepthDS);

	// Set inputs
	ConstantBuffer::SharedPtr pCB = mDebugVisualizationsPass.pVars["DebugPassCB"];
	pCB["farOverNear"] = mpCamera->getFarPlane() / mpCamera->getNearPlane();

	auto projMatrixInv = glm::inverse(mpCamera->getProjMatrix());
	pCB["screenUnprojection"] = glm::float4(projMatrixInv[0][0], projMatrixInv[1][1], projMatrixInv[3][2], 0.0f);
	pCB["screenUnprojection2"] = glm::float4(projMatrixInv[3][0], projMatrixInv[3][1], 0.0f, 1.0f / (projMatrixInv[2][3] + projMatrixInv[3][3]));

	pCB["invView"] = glm::inverse(mpCamera->getViewMatrix());
	pCB["transposeView"] = glm::transpose(mpCamera->getViewMatrix());
	pCB["samplesCount"] = mLightSamplesCount;
	pCB["aspectRatio"] = mpCamera->getAspectRatio();
	pCB["viewportDims"] = vec2(pTargetFbo->getWidth(), pTargetFbo->getHeight());
	pCB["visibilityBufferToDisplay"] = (int)mVisibilityBufferToDisplay;
	pCB["cullingBlockSize"] = (unsigned int)mSamplingMatrixBlockSize;
	pCB["maxVariationLevel"] = mFilteringMaxVariationLevel;

	int historyMaskId = mFrameNumber % 4;
	pCB["frameNumber"] = (unsigned int)historyMaskId;

	mDebugVisualizationsPass.pVars->setTexture("depthBuffer", getCurrentFrameDepthFBO()->getDepthStencilTexture());
	mDebugVisualizationsPass.pVars->setTexture("samplesBuffer", mpPoisson1to32sppSamples);
	mDebugVisualizationsPass.pVars->setTexture("motionAndDepthBuffer", mpMotionAndDepthBuffer);
	mDebugVisualizationsPass.pVars->setTexture("normalsBuffer", mpNormalsBuffer);
	
	mDebugVisualizationsPass.pPass->getProgram()->addDefine("_LIGHT_COUNT", std::to_string(getLightsPerPassNum(0)));

	for (int i = 0; i < getPackedTexturesPerPassNum(0); i++)
	{
		if (mFrameNumber % 2 == 0)
			mDebugVisualizationsPass.pVars->setTexture("variationBuffer[" + std::to_string(i) + "]", mRtFilteredVariationBufferOdd[i]);
		else
			mDebugVisualizationsPass.pVars->setTexture("variationBuffer[" + std::to_string(i) + "]", mRtFilteredVariationBufferEven[i]);
	}

	for (int i = 0; i < getLightsPerPassNum(0); i++) {
		mDebugVisualizationsPass.pVars->setTexture("variationHistoryBuffer[" + std::to_string(i) + "]", mOutputVariationAndSampleCountCache[i]);
	}

	// Kick it off
	pContext->setGraphicsVars(mDebugVisualizationsPass.pVars);
	mDebugVisualizationsPass.pPass->execute(pContext);
}

void DXRShadows::renderScene(RenderContext* pContext, Fbo::SharedPtr pTargetFbo)
{

 	pContext->setGraphicsState(mpGraphicsState);

	if (mDepthNormalsSource == DepthNormalsSourceType::GBufferPass) {

		// Run G-Buffer pass
		GBufferPass(pContext);

	} else {

		// Run depth pre-pass
		depthPass(pContext);

		// Generate motion vectors
		motionVectorsPass(pContext, pTargetFbo);
	}

	// Skybox pass
	renderSkyBox(pContext, pTargetFbo);

	// Shadow mapping pass
	if (mShadowsAlgorithm == ShadowsAlgorithm::ShadowMapping) {
		shadowMappingPass(pContext);
	}

	// Ray traced shadows passes
	if (mShadowsAlgorithm == ShadowsAlgorithm::RayTracing) {

		// Process 8 lights per pass
		for (int pass = 0; pass < getPassesPerSceneNum(); pass++) {

			rayTracedShadowsPass(pContext, pTargetFbo, pass);

			// Shadows filtering pass (denoising)
			visibilityFilteringPass(pContext, pTargetFbo, pass);
		}
	}

	// Lighting pass
	lightingPass(pContext, pTargetFbo);

	// Optional debug pass
	debugVisualizationsPass(pContext, pTargetFbo);


}

void DXRShadows::onFrameRender(SampleCallbacks* pSample, RenderContext* pRenderContext, const std::shared_ptr<Fbo>& pTargetFbo)
{

	pRenderContext->clearFbo(pTargetFbo.get(), kClearColor, 1.0f, 0, FboAttachmentType::All);

	if (mpScene)
	{		
		
		mCamController.update();
		mpRaytracingRenderer->update(pSample->getCurrentTime());
		
		mFrameNumber = (unsigned int) pSample->getFrameID();

		// Setup lights buffer
		updateLightBuffer();

		renderScene(pRenderContext, pTargetFbo);

		mPreviousViewProjectionMatrix = mpCamera->getViewProjMatrix();
		
	}
}

bool DXRShadows::onKeyEvent(SampleCallbacks* pSample, const KeyboardEvent& keyEvent)
{
	if (mCamController.onKeyEvent(keyEvent))
	{
		return true;
	}
	
	return false;
}

bool DXRShadows::onMouseEvent(SampleCallbacks* pSample, const MouseEvent& mouseEvent)
{
	return mCamController.onMouseEvent(mouseEvent);
}

void DXRShadows::onResizeSwapChain(SampleCallbacks* pSample, uint32_t width, uint32_t height)
{
	unsigned int lightCount;

	mRtShadowsPass.varsAlreadySetup = false;

	if (mpScene == nullptr)
		lightCount = 0;
	else
		lightCount = mpScene->getLightCount();

	if (swapChainLightCount == lightCount &&
		swapChainWidth == width &&
		swapChainHeight == height) return;

	swapChainLightCount = lightCount;
	swapChainWidth = width;
	swapChainHeight = height;

	float h = (float)height;
	float w = (float)width;

	float aspectRatio = (w / h);

	if (mpCamera != nullptr) {
		mpCamera->setAspectRatio(aspectRatio);
	}
	
	// Create FBO for depth pre-pass
	Fbo::Desc fboDesc;

	fboDesc.setDepthStencilTarget(ResourceFormat::D32Float);
	fboDesc.setSampleCount(1);
	
	mpOddDepthPassFbo = FboHelper::create2D(width, height, fboDesc);
	mpEvenDepthPassFbo = FboHelper::create2D(width, height, fboDesc);

	// Create FBO for G-Buffer pass
	mpOddGBufferPassFbo = FboHelper::create2D(width, height, fboDesc);
	mpEvenGBufferPassFbo = FboHelper::create2D(width, height, fboDesc);
	
	// Create main FBO for lighting pass
	mpLightingPassFbo = FboHelper::create2D(width, height, fboDesc);

	// Create buffers for ray tracing shadows
	mRtOutputFilteredVisibilityBuffer.clear();
	mOutputVariationAndSampleCountCache.clear();
	mRtVisibilityCacheOdd.clear();
	mRtVisibilityCacheEven.clear();
	mRtTempBuffers.clear();

	for (unsigned int i = 0; i < lightCount; i++) {
		mRtVisibilityCacheOdd.push_back(Texture::create2D(width, height, ResourceFormat::RGBA16Float, 1, 1, nullptr, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::ShaderResource));
		mRtVisibilityCacheEven.push_back(Texture::create2D(width, height, ResourceFormat::RGBA16Float, 1, 1, nullptr, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::ShaderResource));
		mOutputVariationAndSampleCountCache.push_back(Texture::create2D(width, height, ResourceFormat::RGBA16Float, 1, 4, nullptr, Resource::BindFlags::UnorderedAccess | Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget));
	}

	mpMotionAndDepthBuffer = Texture::create2D(width, height, ResourceFormat::RGBA32Float, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget | Resource::BindFlags::UnorderedAccess);
	mpNormalsBuffer = Texture::create2D(width, height, ResourceFormat::RGBA32Float, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget);

	mpMotionVectorsPassFbo = Fbo::create();
	mpMotionVectorsPassFbo->attachColorTarget(mpMotionAndDepthBuffer, 0);
	mpMotionVectorsPassFbo->attachColorTarget(mpNormalsBuffer, 1);

	// Attach render targets for G-Buffer pass
	mpOddGBufferPassFbo->attachColorTarget(mpMotionAndDepthBuffer, 0);
	mpOddGBufferPassFbo->attachColorTarget(mpNormalsBuffer, 1);

	mpEvenGBufferPassFbo->attachColorTarget(mpMotionAndDepthBuffer, 0);
	mpEvenGBufferPassFbo->attachColorTarget(mpNormalsBuffer, 1);

	// Setup filtering FBOs
	mpVisibilityFilteringPass1Fbo = Fbo::create();
	mpVisibilityFilteringPass2Fbo = Fbo::create();

	mpVariationFilteringPass1Fbo = Fbo::create();
	mpVariationFilteringPass2Fbo = Fbo::create();
	
	if (lightCount > 0) {
		int packedTexturesCount = getPackedTexturesPerPassNum(0);
		int totalPackedTexturesCount = getPackedTexturesPerSceneNum();

		if (totalPackedTexturesCount % 2 == 1) totalPackedTexturesCount++;

		for (int i = 0; i < totalPackedTexturesCount; i++) {
			mRtOutputFilteredVisibilityBuffer.push_back(Texture::create2D(width, height, ResourceFormat::RGBA16Float, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget | Resource::BindFlags::UnorderedAccess));
			mRtFilteredVariationBufferEven.push_back(Texture::create2D(width, height, ResourceFormat::RGBA16Float, 1, Resource::kMaxPossible, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget | Resource::BindFlags::UnorderedAccess));
			mRtFilteredVariationBufferOdd.push_back(Texture::create2D(width, height, ResourceFormat::RGBA16Float, 1, Resource::kMaxPossible, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget | Resource::BindFlags::UnorderedAccess));
		}

		for (int i = 0; i < 2; i++) {
			mRtTempBuffers.push_back(Texture::create2D(width, height, ResourceFormat::RGBA16Float, 1, 1, nullptr, Resource::BindFlags::ShaderResource | Resource::BindFlags::RenderTarget));

			mpVisibilityFilteringPass1Fbo->attachColorTarget(mRtTempBuffers[i], i);
			mpVisibilityFilteringPass2Fbo->attachColorTarget(mRtOutputFilteredVisibilityBuffer[i], i);

			mpVariationFilteringPass1Fbo->attachColorTarget(mRtTempBuffers[i], i);
		}
	}

	for (auto it = mShadowPass.mpCsmTech.begin(); it != mShadowPass.mpCsmTech.end(); ++it)
	{
		(*it)->onResize(width, height);
	}

}

Fbo::SharedPtr DXRShadows::getCurrentFrameDepthFBO() {
	bool mGBufferPass = true;

	if (mGBufferPass) {
		if (mFrameNumber % 2 == 0)
			return mpEvenGBufferPassFbo;
		else
			return mpOddGBufferPassFbo;
	} else {
		if (mFrameNumber % 2 == 0)
			return mpEvenDepthPassFbo;
		else
			return mpOddDepthPassFbo;
	}
}

Fbo::SharedPtr DXRShadows::getPreviousFrameDepthFBO() {
	bool mGBufferPass = true;

	if (mGBufferPass) {
		if (mFrameNumber % 2 == 1)
			return mpEvenGBufferPassFbo;
		else
			return mpOddGBufferPassFbo;
	} else {
		if (mFrameNumber % 2 == 1)
			return mpEvenDepthPassFbo;
		else
			return mpOddDepthPassFbo;
	}
}

void DXRShadows::applyDefine(Program::SharedPtr pProgram, bool enabled, std::string define, std::string value)
{
	if (enabled)
		pProgram->addDefine(define, value);
	else
		pProgram->removeDefine(define);
}

void DXRShadows::applyDefine(RtProgram::SharedPtr pProgram, bool enabled, std::string define, std::string value)
{
	if (enabled)
		pProgram->addDefine(define, value);
	else
		pProgram->removeDefine(define);
}

int DXRShadows::getPassesPerSceneNum() {

	int lightsInScene = mpScene->getLightCount();

	return ((lightsInScene - 1) / MAX_LIGHTS_PER_PASS) + 1;
}

int DXRShadows::getLightsPerPassNum(int passNum) {

	int lightsInScene = mpScene->getLightCount();

	if (MAX_LIGHTS_PER_PASS >= lightsInScene) return lightsInScene;

	return min(MAX_LIGHTS_PER_PASS, lightsInScene - passNum * MAX_LIGHTS_PER_PASS);
}

int DXRShadows::getPackedTexturesPerSceneNum() {

	return ((mpScene->getLightCount() - 1) / 4 + 1);
}

int DXRShadows::getPackedTexturesPerPassNum(int passNum) {

	int lightsPerPass = getLightsPerPassNum(passNum);

	return (lightsPerPass - 1) / 4 + 1;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
	DXRShadows::UniquePtr pRenderer = std::make_unique<DXRShadows>();
	
	auto token = strtok(lpCmdLine, " ");
	SampleConfig config;

	if (token != nullptr) {
		kDefaultScene = std::string(token);
	}
	
	config.windowDesc.title = "DXRShadowsGem";
	config.windowDesc.resizableWindow = true;
	config.deviceDesc.enableVsync = false;

	Sample::run(config, pRenderer);
}
