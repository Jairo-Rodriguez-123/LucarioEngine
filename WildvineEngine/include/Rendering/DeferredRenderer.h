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

class
  DeferredRenderer : public ISceneRenderer {
public:
  /*
    *  @brief Initialize renderer resources with the provided device.
  */
  HRESULT
    init(Device& device) override;

  /*
    *  @brief Resize internal resources to the given width and height.
  */
  void
    resize(Device& device, unsigned int width, unsigned int height) override;

  /*
    *  @brief Resize internal resources to the given width and height (overload).
  */
  void
    resize(Device& device, unsigned int width, unsigned int height) override;

  /*
    *  @brief Render the scene using the provided device context, camera and viewport pass.
  */
  void
    render(DeviceContext& deviceContext,
      const Camera& camera,
      const RenderScene& scene,
      EditorViewportPass viewportPass) override;

  /*
    *  @brief Release and destroy all allocated renderer resources.
  */
  void
    destroy() override;

  /*
    *  @brief Get shader resource view for the shadow map.
  */
  ID3D11ShaderResourceView*
    getShadowMapSRV() const override { return m_shadowDepthSRV.m_textureFromImg; }

  /*
    *  @brief Get shader resource view used for pre-shadow debug visualization.
  */
  ID3D11ShaderResourceView*
    getPreShadowSRV() const override { return m_preShadowDebugPass.getSRV(); }

  /*
    *  @brief Get shader resource view for the G-Buffer containing albedo and metallic.
  */
  ID3D11ShaderResourceView*
    getGBufferAlbedoMetallicSRV() const override { return m_gBufferAlbedoMetallicSRV.m_textureFromImg; }

  /*
    *  @brief Get shader resource view for the G-Buffer containing normal and roughness.
  */
  ID3D11ShaderResourceView*
    getGBufferNormalRoughnessSRV() const override { return m_gBufferNormalRoughnessSRV.m_textureFromImg; }

  /*
    *  @brief Get shader resource view for the G-Buffer containing world-space position / AO / spec data.
  */
  ID3D11ShaderResourceView*
    getGBufferWorldAoSVR() const override { return m_gBufferWorldAoSRV.m_textureFromImg; }

  /*
    *  @brief Get shader resource view for the G-Buffer containing emissive and alpha channels.
  */
  ID3D11ShaderResourceView*
    getGBufferEmissiveAlphaSRV() const override { return m_gBufferEmissiveAlphaSRV.m_textureFromImg; }

  /*
    *  @brief Enable or disable shadow factor debug visualization.
  */
  void
    setShadowFactorDebugEnabled(bool enabled) override { m_shadowFactorDebugEnabled = enabled; }

  /*
    *  @brief Set the debug view mode for deferred rendering.
  */
  void
    setDeferredDebugViewMode(int mode) override { m_deferredDebugViewMode = mode; }

  /*
    *  @brief Return a debug name identifying this renderer.
  */
  const char*
    getDebugName() const override { return "DeferredRenderer"; }

private:
  /*
    *  @brief Build rendering queues (opaque/transparent) from the scene for the given camera.
  */
  void buildQueues(RenderScene& scene, const Camera& camera);
  /*
    *  @brief Update per-frame constant buffers and GPU state using camera and scene data.
  */
  void updatePerFrame(const Camera& camera, const RenderScene& scene, DeviceContext& deviceContext);
  /*
    *  @brief Compute or update light matrices (e.g., shadow matrices) for the current frame.
  */
  void updateLightMatrices(const Camera& camera, const RenderScene& scene);
  /*
    *  @brief Render the provided scene into the specified target pass, optionally applying shadows.
  */
  void renderSceneToTarget(DeviceContext& deviceContext, RenderScene& scene, EditorViewportPass& targetPass, bool applyShadows);
  /*
    *  @brief Bind G-Buffer render targets and the provided depth stencil view to the pipeline.
  */
  void bindGBufferTargets(DeviceContext& deviceContext, ID3D11DepthStencilView);
  /*
    *  @brief Bind the final render target and depth stencil view for presentation or post-processing.
  */
  void binFinalTarget(DeviceContext& deviceContext, ID3D11RenderTargetView* renderTargetView, ID3D11DepthStencilView* depthStencilView);
  /*
    *  @brief Clear shader resource views used by deferred rendering to avoid read/write hazards.
  */
  void clearDeferredSRVs(DeviceContext& deviceContext);
  /*
    *  @brief Execute the geometry pass populating the G-Buffer.
  */
  void renderGeometryPass(DeviceContext& deviceContext);
  /*
    *  @brief Render a single geometry object during the geometry pass.
  */
  void renderGeometryObject(DeviceContext& deviceContext, const RenderObject& object);
  /*
    *  @brief Execute the lighting pass reading from G-Buffer and shading pixels.
  */
  void renderLightingPass(DeviceContext& deviceContext);
  /*
    *  @brief Render the scene skybox.
  */
  void renderSkyboxPass(DeviceContext& deviceContext, RenderScene& scene);
  /*
    *  @brief Render transparent geometry after opaque passes.
  */
  void renderTransparentPass(DeviceContext& deviceContext);
  /*
    *  @brief Render a forward-rendered object for the specified pass type.
  */
  void renderFowardObject(DeviceContext& deviceContext, const RenderObject& object, RenderPassType passType);
  /*
    *  @brief Execute shadow map generation pass.
  */
  void renderShadowPass(DeviceContext& deviceContext);
  /*
    *  @brief Render a single object into the shadow map.
  */
  void renderShadowObject(DeviceContext& deviceContext, const RenderObject& object);
  /*
    *  @brief Create resources required for shadow mapping (textures, views, states).
  */
  HRESULT createShadowResources(Device& device);
  /*
    *  @brief Create G-Buffer textures, SRVs and RTVs for the given resolution.
  */
  HRESULT createGBufferResources(Device& device, unsigned int width, unsigned int height);
  /*
    *  @brief Helper to create a specific G-Buffer target with the provided format.
  */
  HRESULT createGBufferTarget(Device& device,
      unsigned int width,
      unsigned int height,      
      DXGI_FORMAT format,
      Texture& texture,
      Texture& srv,
      RenderTargetView& rtv);

  /*
    *  @brief Create GPU resources related to scene lights (buffers, UAVs, etc.).
  */
  HRESULT CreateLightResources(Device& device);
  /*
    *  @brief Create a full-screen quad vertex/index buffers for post-processing.
  */
  HRESULT CreateFullScreenQuad(Device& device);
  /*
    *  @brief Create blend states used by the renderer for various material blends.
  */
  HRESULT CreateBlenderStates(Device& device);
  /*
    *  @brief Resolve and return the appropriate blend state for the provided material.
  */
  ID3D11BlendState* resolveBlendState(const Material* material) const;

private:
  /*
    *  @brief Per-frame constant buffer data.
  */
  Buffer m_perFrameBuffer;
  /*
    *  @brief Per-object constant buffer data.
  */
  Buffer m_perObjectBuffer;
  /*
    *  @brief Per-material constant buffer data.
  */
  Buffer m_perMaterialBuffer;
  /*
    *  @brief Buffer used for lighting debug data.
  */
  Buffer m_lightingDebugBuffer;
  /*
    *  @brief Vertex buffer for a full-screen quad.
  */
  Buffer m_fullscreenVertexBuffer;
  /*
    *  @brief Index buffer for a full-screen quad.
  */
  Buffer m_fullscreenIndexBuffer;

  /*
    *  @brief Depth stencil state configured for transparent objects.
  */
  DepthStencilState m_transparentDepthStencil;
  /*
    *  @brief Depth stencil state that is disabled (no depth test/write).
  */
  DepthStencilState m_disabledDepthStencil;
  /*
    *  @brief Depth stencil state used when rendering the shadow map.
  */
  DepthStencilState m_shadowDepthStencil;

  /*
    *  @brief Blend state for alpha blending.
  */
  ID3D11BlendState* m_alphaBlendState = nullptr;
  /*
    *  @brief Blend state for opaque rendering.
  */
  ID3D11BlendState* m_opaqueBlendState = nullptr;
  /*
    *  @brief Blend state for additive blending.
  */
  ID3D11BlendState* m_additiveBlendState = nullptr;
  /*
    *  @brief Blend state for premultiplied alpha blending.
  */
  ID3D11BlendState* m_premultipliedBlendState = nullptr;
  /*
    *  @brief Blend factor array used when setting blend state.
  */
  float m_blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

  /*
    *  @brief Texture holding the shadow depth buffer.
  */
  Texture m_shadowDepthTexture;
  /*
    *  @brief Shader resource view wrapper for sampling the shadow depth texture.
  */
  Texture m_shadowDepthSRV;
  /*
    *  @brief Depth stencil view for writing the shadow depth.
  */
  DepthStencilView m_shadowDSV;
  /*
    *  @brief Shader program used to render shadow casters.
  */
  ShaderProgram m_shadowShader;
  /*
    *  @brief Rasterizer state specialized for shadow rendering (e.g., cull settings).
  */
  RasterizerState m_shadowRasterizer;
  /*
    *  @brief Resolution size (width/height) for the shadow map.
  */
  unsigned int m_shadowMapSize = 2048;

  /*
    *  @brief Shader program used to populate the G-Buffer.
  */
  ShaderProgram m_gBufferShader;
  /*
    *  @brief Shader program implementing deferred lighting pass.
  */
  ShaderProgram m_defferedLightingShader;
  /*
    *  @brief Sampler state used during lighting (shadow/G-Buffer sampling).
  */
  SamplerState m_ligthingSampler;
  /*
    *  @brief Rasterizer state for full-screen passes.
  */
  RasterizerState m_fullscreenRasterizer;

  /*
    *  @brief G-Buffer texture storing albedo and metallic channels.
  */
  Texture m_gBufferAlbedoMetallicTexture;
  /*
    *  @brief SRV wrapper for the albedo/metallic G-Buffer.
  */
  Texture m_gBufferAlbedoMetallicSRV;
  /*
    *  @brief Render target view for the albedo/metallic G-Buffer.
  */
  RenderTargetView m_gBufferAlbedoMetalicRTV;

  /*
    *  @brief G-Buffer texture storing normal and roughness channels.
  */
  Texture m_gBufferNormalRoughnessTexture;
  /*
    *  @brief SRV wrapper for the normal/roughness G-Buffer.
  */
  Texture m_gBufferNormalRoughnessSRV;
  /*
    *  @brief Render target view for the normal/roughness G-Buffer.
  */
  RenderTargetView m_gBufferNormalRoughnessRTV;

  /*
    *  @brief G-Buffer texture storing world-space data and ambient occlusion.
  */
  Texture m_gBufferWorldAoTexture;
  /*
    *  @brief SRV wrapper for the world-space/AO G-Buffer.
  */
  Texture m_gBufferWorldAoSRV;
  /*
    *  @brief Render target view for the world-space/AO G-Buffer.
  */
  RenderTargetView m_gBufferWorldAoRTV;

  /*
    *  @brief G-Buffer texture storing emissive and alpha channels.
  */
  Texture m_gBufferEmissiveAlphaTexture;
  /*
    *  @brief SRV wrapper for the emissive/alpha G-Buffer.
  */
  Texture m_gBufferEmissiveAlphaSRV;
  /*
    *  @brief Render target view for the emissive/alpha G-Buffer.
  */
  RenderTargetView m_gBufferEmissiveAlphaRTV;

  /*
    *  @brief Editor viewport pass used to preview pre-shadow content for debugging.
  */
  EditorViewportPass m_preShadowDebugPass;
  /*
    *  @brief Whether shadows should be applied during rendering.
  */
  bool m_applyShadows = true;
  /*
    *  @brief Current render target width.
  */
  unsigned int m_renderWidth = 1280;
  /*
    *  @brief Current render target height.
  */
  unsigned int m_renderHeight = 720;

  /*
    *  @brief CPU-side copy of per-frame constant buffer values.
  */
  CBPerFrame m_cbPerFrame{};
  /*
    *  @brief CPU-side copy of per-object constant buffer values.
  */
  CBPerObject m_cbPerObject{};
  /*
    *  @brief CPU-side copy of per-material constant buffer values.
  */
  CBPerMaterial m_cbPerMaterial{};
  /*
    *  @brief Debug structure containing lighting debug settings and padding.
  */
  struct DeferredLigthingDebugData {
    int debugViewMode = 0;
    int shadowStrength = 0;
    float pad0 = 0.0f;
    float pad1 = 0.0f;
  } m_lightingDebugData{};
  /*
    *  @brief Toggle to enable shadow factor debug overlay.
  */
  bool m_shadowFactorDebugEnabled = false;
  /*
    *  @brief Selected debug view mode for deferred output visualization.
  */
  int m_deferredDebugViewMode = 0;

  /*
    *  @brief Queue of opaque objects to be rendered in the main passes.
  */
  std::vector<const RenderObject*> m_opaqueQueue;
  /*
    *  @brief Queue of transparent objects to be rendered after opaque geometry.
  */
  std::vector<const RenderObject*> m_transparentQueue;
};
