#pragma once
#include "Prerequisites.h"
#include "Buffer.h"
#include "DepthStencilState.h"
#include "DepthStencilView.h"
#include "RasterizerState.h"
#include "Rendering/ISceneRenderer.h"
#include "Rendering/RenderScene.h"
#include "Rendering/RenderTypes.h"
#include "ShaderProgram.h"
#include "Texture.h"
#include "EngineUtilities/Utilities/EditorViewportPass.h"

class Device;
class DeviceContext;
class Camera;
class Material;


class
ForwardRenderer : public ISceneRenderer {
public:
	~ForwardRenderer() override { destroy(); }


	/*
	 *  @brief Initializes renderer resources using the provided device.
	*/
	HRESULT init(Device& device) override;

	/*
	 *  @brief Resize internal render targets and resources to the provided dimensions.
	*/
	HRESULT resize(Device& device, unsigned int width, unsigned int height) override;

	/*
	 *  @brief Updates per-frame constant buffers and internal state based on the camera and scene.
	*/
	void updatePerFrame(const Camera& camera, const RenderScene& scene, DeviceContext& deviceContext);

	/*
	 *  @brief Executes the full forward rendering pipeline for the given scene and viewport.
	*/
	void 
	render(DeviceContext& deviceContext,
		const Camera& camera,
		RenderScene& scene,
		EditorViewportPass& viewportPass) override;

	/*
	 *  @brief Releases all GPU and CPU resources owned by the renderer.
	*/
	void destroy() override;

	/*
	 *  @brief Returns a shader resource view for the generated shadow depth texture.
	*/
	ID3D11ShaderResourceView* getShadowMapSRV() const override { return m_shadowDepthSRV.m_textureFromImg; }

	/*
	 *  @brief Returns a shader resource view used for pre-shadow debug visualization.
	*/
	ID3D11ShaderResourceView* getPreShadowSRV() const override { return m_preShadowDebugPass.getSRV(); }

	const char* getDebugName() const override { return "ForwardRenderer"; }

private:
	/*
	 *  @brief Builds opaque and transparent render queues from the scene using camera visibility.
	*/
	void buildQueues(RenderScene& scene, const Camera& camera);

	/*
	 *  @brief Renders a debug pass that visualizes pre-shadow results into a debug viewport.
	*/
	void renderPreShadowDebugPass(DeviceContext& deviceContext, RenderScene& scene);

	/*
	 *  @brief Renders the shadow map by drawing shadow casters into the shadow depth target.
	*/
	void renderShadowPass(DeviceContext& deviceContext);

	/*
	 *  @brief Renders all opaque objects in the opaque queue with depth-write and no blending.
	*/
	void renderOpaquePass(DeviceContext& deviceContext);

	/*
	 *  @brief Renders transparent objects sorted and blended according to their materials.
	*/
	void renderTransparentPass(DeviceContext& deviceContext);

	/*
	 *  @brief Renders the scene skybox using the provided scene data.
	*/
	void renderSkyboxPass(DeviceContext& deviceContext, RenderScene& scene);

	/*
	 *  @brief Renders a single object for the specified pass type (opaque/transparent/etc.).
	*/
	void renderObject(DeviceContext& deviceContext, const RenderObject& object, RenderPassType passType);

	/*
	 *  @brief Renders a single object into the shadow map using the shadow shader.
	*/
	void renderShadowObject(DeviceContext& deviceContext, const RenderObject& object);

	/*
	 *  @brief Allocates and initializes GPU resources required for shadow mapping.
	*/
	HRESULT createShadowResources(Device& device);

	/*
	 *  @brief Computes and updates light view/projection matrices used for shadow mapping.
	*/
	void updateLightMatrices(const Camera& camera, const RenderScene& scene);

	/*
	 *  @brief Creates blend state objects used for different material blending modes.
	*/
	HRESULT createBlendStates(Device& device);

	/*
	 *  @brief Chooses and returns the appropriate blend state for the given material.
	*/
	ID3D11BlendState* resolveBlendState(const Material* material) const;

private:
	/*
	 *  @brief Per-frame constant buffer containing camera and lighting data.
	*/
	Buffer m_perFrameBuffer;
	/*
	 *  @brief Per-object constant buffer used to upload object transform and related data.
	*/
	Buffer m_perObjectBuffer;
	/*
	 *  @brief Per-material constant buffer for material-specific parameters.
	*/
	Buffer m_perMaterialBuffer;
	/*
	 *  @brief Depth-stencil state configured for rendering transparent objects.
	*/
	DepthStencilState m_transparentDepthStencil;
	/*
	 *  @brief Depth state used by the shadow pass; depth testing and writes are enabled.
	 */
	DepthStencilState m_shadowDepthStencil;
	/*
	 *  @brief Blend state used for standard alpha blending.
	*/
	ID3D11BlendState* m_alphaBlendState = nullptr;
	/*
	 *  @brief Blend state used for opaque (no blending) rendering.
	*/
	ID3D11BlendState* m_opaqueBlendState = nullptr;
	/*
	 *  @brief Blend state used for additive blending.
	*/
	ID3D11BlendState* m_additiveBlendState = nullptr;
	/*
	 *  @brief Blend state used for premultiplied alpha blending.
	*/
	ID3D11BlendState* m_premultipliedBlendState = nullptr;
	/*
	 *  @brief Blend factor array passed when setting certain blend states.
	*/
	float m_blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	/*
	 *  @brief Texture resource storing the shadow depth map.
	*/
	Texture m_shadowDepthTexture;
	/*
	 *  @brief Shader resource view wrapper for the shadow depth texture.
	*/
	Texture m_shadowDepthSRV;
	/*
	 *  @brief Depth-stencil view used to render into the shadow depth texture.
	*/
	DepthStencilView m_shadowDSV;
	/*
	 *  @brief Shader program used for rendering shadow casters.
	*/
	ShaderProgram m_shadowShader;
	/*
	 *  @brief Rasterizer state configured for shadow map rendering (e.g., culling, bias).
	*/
	RasterizerState m_shadowRasterizer;
	/*
	 *  @brief Resolution (width and height) of the square shadow map texture.
	*/
	unsigned int m_shadowMapSize = 2048;
	/*
	 *  @brief Utility pass that exposes pre-shadow data for debugging in the editor viewport.
	*/
	EditorViewportPass m_preShadowDebugPass;
	/*
	 *  @brief Flag toggling whether shadows are applied during rendering.
	*/
	bool m_applyShadows = true;
	/*
	 *  @brief True when the current scene contains a directional light allowed to cast shadows.
	 */
	bool m_hasShadowCastingLight = false;

	/*
	 *  @brief CPU-side mirror of per-frame constant buffer data.
	*/
	CBPerFrame m_cbPerFrame{};
	/*
	 *  @brief CPU-side mirror of per-object constant buffer data.
	*/
	CBPerObject m_cbPerObject{};
	/*
	 *  @brief CPU-side mirror of per-material constant buffer data.
	*/
	CBPerMaterial m_cbPerMaterial{};

	/*
	 *  @brief Ordered list of pointers to opaque renderable objects.
	*/
	std::vector<const RenderObject*> m_opaqueQueue;
	/*
	 *  @brief Ordered list of pointers to transparent renderable objects.
	*/
	std::vector<const RenderObject*> m_transparentQueue;
};


