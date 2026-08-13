/**
 * @file DeferredRenderer.h
 * @brief Declara la API de DeferredRenderer dentro del subsistema Rendering.
 * @ingroup rendering
 */
#pragma once
#include "Buffer.h"
#include "DepthStencilState.h"
#include "DepthStencilView.h"
#include "RasterizerState.h"
#include "Rendering/ISceneRenderer.h"
#include "Rendering/RenderScene.h"
#include "Rendering/RenderTypes.h"
#include "SamplerState.h"
#include "ShaderProgram.h"
#include "Texture.h"
#include "EngineUtilities/Utilities/EditorViewportPass.h"

class Device;
class DeviceContext;
class Camera;
class Material;

/**
 * @class DeferredRenderer
 * @brief Implementa un pipeline diferido con GBuffer y lighting pass.
 *
 * El renderer usa deferred shading para superficies opacas y mantiene un subpass
 * forward para transparencias, de modo que el pipeline del editor siga funcionando
 * con el contenido actual del engine.
 */
class
	DeferredRenderer : public ISceneRenderer {
public:
	~DeferredRenderer() override { destroy(); }

	HRESULT
		init(Device& device) override;

	HRESULT
		resize(Device& device, unsigned int width, unsigned int height) override;

	void
		render(DeviceContext& deviceContext,
			const Camera& camera,
			RenderScene& scene,
			EditorViewportPass& viewportPass) override;

	void
		destroy() override;

	ID3D11ShaderResourceView*
		getShadowMapSRV() const override { return m_shadowDepthSRV.m_textureFromImg; }

	ID3D11ShaderResourceView*
		getPreShadowSRV() const override { return m_preShadowDebugPass.getSRV(); }

	ID3D11ShaderResourceView*
		getGBufferAlbedoMetallicSRV() const override { return m_gBufferAlbedoMetallicSRV.m_textureFromImg; }

	ID3D11ShaderResourceView*
		getGBufferNormalRoughnessSRV() const override { return m_gBufferNormalRoughnessSRV.m_textureFromImg; }

	ID3D11ShaderResourceView*
		getGBufferWorldAoSRV() const override { return m_gBufferWorldAoSRV.m_textureFromImg; }

	ID3D11ShaderResourceView*
		getGBufferEmissiveAlphaSRV() const override { return m_gBufferEmissiveAlphaSRV.m_textureFromImg; }

	void
		setShadowFactorDebugEnabled(bool enabled) override { m_shadowFactorDebugEnabled = enabled; }

	void
		setDeferredDebugViewMode(int mode) override { m_deferredDebugViewMode = mode; }

	void setPostProcessEnabled(bool enabled) { m_postProcessData.PostProcessEnabled = enabled ? 1 : 0; }
	void setBloomEnabled(bool enabled) { m_postProcessData.BloomEnabled = enabled ? 1 : 0; }
	void setTonemappingEnabled(bool enabled) { m_postProcessData.TonemappingEnabled = enabled ? 1 : 0; }
	void setFXAAEnabled(bool enabled) { m_postProcessData.FXAAEnabled = enabled ? 1 : 0; }
	void setBloomThreshold(float value) { m_postProcessData.BloomThreshold = value; }
	void setBloomIntensity(float value) { m_postProcessData.BloomIntensity = value; }
	void setExposure(float value) { m_postProcessData.Exposure = value; }
	void setFXAAStrength(float value) { m_postProcessData.FXAAStrength = value; }

	const char*
		getDebugName() const override { return "DeferredRenderer"; }

private:
	void buildQueues(RenderScene& scene, const Camera& camera);
	void updatePerFrame(const Camera& camera, const RenderScene& scene, DeviceContext& deviceContext);
	void updateLightMatrices(const Camera& camera, const RenderScene& scene);
	void renderSceneToTarget(DeviceContext& deviceContext, RenderScene& scene, EditorViewportPass& targetPass, bool applyShadows);
	void bindGBufferTargets(DeviceContext& deviceContext, ID3D11DepthStencilView* depthStencilView);
	void bindFinalTarget(DeviceContext& deviceContext, ID3D11RenderTargetView* renderTargetView, ID3D11DepthStencilView* depthStencilView);
	void clearDeferredSRVs(DeviceContext& deviceContext);
	void renderGeometryPass(DeviceContext& deviceContext);
	void renderGeometryObject(DeviceContext& deviceContext, const RenderObject& object);
	void renderLightingPass(DeviceContext& deviceContext);
	void renderPostProcessStack(DeviceContext& deviceContext, ID3D11RenderTargetView* finalRenderTarget);
	void drawPostProcessStage(DeviceContext& deviceContext, ShaderProgram& shader, ID3D11ShaderResourceView* source, ID3D11RenderTargetView* target);
	void renderSkyboxPass(DeviceContext& deviceContext, RenderScene& scene);
	void renderTransparentPass(DeviceContext& deviceContext);
	void renderForwardObject(DeviceContext& deviceContext, const RenderObject& object, RenderPassType passType);
	void renderShadowPass(DeviceContext& deviceContext);
	void renderShadowObject(DeviceContext& deviceContext, const RenderObject& object);
	HRESULT createShadowResources(Device& device);
	HRESULT createGBufferResources(Device& device, unsigned int width, unsigned int height);
	HRESULT createGBufferTarget(Device& device,
		unsigned int width,
		unsigned int height,
		DXGI_FORMAT format,
		Texture& texture,
		Texture& srv,
		RenderTargetView& rtv);
	HRESULT createLightingResources(Device& device);
	HRESULT createPostProcessResources(Device& device);
	HRESULT createPostProcessTarget(Device& device, unsigned int width, unsigned int height);
	HRESULT createFullScreenQuad(Device& device);
	HRESULT createBlendStates(Device& device);
	ID3D11BlendState* resolveBlendState(const Material* material) const;

private:
	Buffer m_perFrameBuffer;
	Buffer m_perObjectBuffer;
	Buffer m_perMaterialBuffer;
	Buffer m_lightingDebugBuffer;
	Buffer m_postProcessBuffer;
	Buffer m_fullscreenVertexBuffer;
	Buffer m_fullscreenIndexBuffer;

	DepthStencilState m_transparentDepthStencil;
	DepthStencilState m_disabledDepthStencil;
	DepthStencilState m_shadowDepthStencil;

	ID3D11BlendState* m_alphaBlendState = nullptr;
	ID3D11BlendState* m_opaqueBlendState = nullptr;
	ID3D11BlendState* m_additiveBlendState = nullptr;
	ID3D11BlendState* m_premultipliedBlendState = nullptr;
	float m_blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	Texture m_shadowDepthTexture;
	Texture m_shadowDepthSRV;
	DepthStencilView m_shadowDSV;
	ShaderProgram m_shadowShader;
	RasterizerState m_shadowRasterizer;
	unsigned int m_shadowMapSize = 2048;

	ShaderProgram m_gBufferShader;
	ShaderProgram m_deferredLightingShader;
	ShaderProgram m_bloomShader;
	ShaderProgram m_tonemapShader;
	ShaderProgram m_fxaaShader;
	SamplerState m_lightingSampler;
	RasterizerState m_fullscreenRasterizer;

	Texture m_gBufferAlbedoMetallicTexture;
	Texture m_gBufferAlbedoMetallicSRV;
	RenderTargetView m_gBufferAlbedoMetallicRTV;

	Texture m_gBufferNormalRoughnessTexture;
	Texture m_gBufferNormalRoughnessSRV;
	RenderTargetView m_gBufferNormalRoughnessRTV;

	Texture m_gBufferWorldAoTexture;
	Texture m_gBufferWorldAoSRV;
	RenderTargetView m_gBufferWorldAoRTV;

	Texture m_gBufferEmissiveAlphaTexture;
	Texture m_gBufferEmissiveAlphaSRV;
	RenderTargetView m_gBufferEmissiveAlphaRTV;

	// HDR scene target used as the input of the deferred post-process stack.
	Texture m_hdrSceneTexture;
	Texture m_hdrSceneSRV;
	RenderTargetView m_hdrSceneRTV;
	Texture m_postPingTexture;
	Texture m_postPingSRV;
	RenderTargetView m_postPingRTV;
	Texture m_postPongTexture;
	Texture m_postPongSRV;
	RenderTargetView m_postPongRTV;

	EditorViewportPass m_preShadowDebugPass;
	bool m_applyShadows = true;
	bool m_hasShadowCastingLight = false;
	unsigned int m_renderWidth = 1280;
	unsigned int m_renderHeight = 720;

	CBPerFrame m_cbPerFrame{};
	CBPerObject m_cbPerObject{};
	CBPerMaterial m_cbPerMaterial{};
	struct DeferredLightingDebugData {
		int DebugViewMode = 0;
		float ShadowStrength = 1.0f;
		float pad0 = 0.0f;
		float pad1 = 0.0f;
	} m_lightingDebugData{};
	bool m_shadowFactorDebugEnabled = false;
	int m_deferredDebugViewMode = 0;

	struct PostProcessData {
		XMFLOAT2 InverseResolution = XMFLOAT2(1.0f / 1280.0f, 1.0f / 720.0f);
		float Exposure = 1.0f;
		float BloomThreshold = 0.85f;
		float BloomIntensity = 0.65f;
		float FXAAStrength = 1.0f;
		int BloomEnabled = 1;
		int TonemappingEnabled = 1;
		int FXAAEnabled = 1;
		int PostProcessEnabled = 1;
		float Gamma = 1.0f;
		float Padding0 = 0.0f;
	} m_postProcessData{};

	std::vector<const RenderObject*> m_opaqueQueue;
	std::vector<const RenderObject*> m_transparentQueue;
};