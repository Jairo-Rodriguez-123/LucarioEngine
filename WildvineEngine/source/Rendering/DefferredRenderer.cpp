#include "Rendering/DeferredRenderer.h"
#include <algorithm>
#include <cmath>
#include "Device.h"
#include "DeviceContext.h"
#include "EngineUtilities/Utilities/Camera.h"
#include "EngineUtilities/Utilities/LayoutBuilder.h"
#include "EngineUtilities/Utilities/Skybox.h"
#include "Rendering/Material.h"
#include "Rendering/MaterialInstance.h"
#include "Rendering/Mesh.h"

namespace MyNamespace
{
  constexpr unsigned int kGBufferTargetCoutnt = 4;

  struct RenderTargetViewAccess {
    ID3D11RenderTargetView* m_renderTargetView = nullptr;
  };

  struct EditorViewportPassAccess {
    Texture m_colorTexture;
    Texture m_colorSRV;
    RenderTargetView m_rtv;
    Texture m_depthTexture;
    DepthStencilView m_dsv;
    unsigned int m_width = 1;
    unsigned int m_height = 1;
  };

  ID3D11RenderTargetView* ResolveViewportRTV(EditorViewportPass& pass) {
    EditorViewportPassAccess* access = reinterpret_cast<EditorViewportPassAccess*>(&pass);
    RenderTargetViewAccess* rtvAccess = reinterpret_cast<RenderTargetViewAccess*>(&access->m_rtv);
    return rtvAccess->m_renderTargetView;
  }

  ID3D11DepthStencilView* ResolveViewportDSV(EditorViewportPass& pass) {
    EditorViewportPassAccess* access = reinterpret_cast<EditorViewportPassAccess*>(&pass);
    return access->m_dsv.m_depthStencilView;
  }

  ID3D11RenderTargetView* ResolveRTV(RenderTargetView& view) {
    RenderTargetViewAccess* access = reinterpret_cast<RenderTargetViewAccess*>(&view);
    return access->m_renderTargetView;
  }

  const LightData*
    findPrimaryShadowLight(const RenderScene& scene) {
    for (const LightData& light : scene.directionalLights) {
      if (light.type == LightType::Directional) {
        return &light;
      }
    }
    return scene.directionalLights.empty() ? nullptr : &scene.directionalLights.front();
  }

  void
    writeLightToFrameBuffer(CBPerFrame& buffer, int lightIndex, const LightData& light) {
    const float range = light.range > 0.0f ? light.range : 10.0f;
    const EU::Vector3 lightColor = light.color * light.intensity;
    buffer.LigthPositionsRanges[lightIndex] = XMFLOAT4(light.position.x, light.position.y, light.position.z, range);
    buffer.LigthColorsTypes[lightIndex] = XMFLOAT4(lightColor.x, lightColor.y, lightColor.z, static_cast<float>(static_cast<int>(light.type)));
    buffer.LightDirectionsIntensities[lightIndex] = XMFLOAT4(light.direction.x, light.direction.y, light.direction.z, light.intensity);
  }
}

HRESULT
DeferredRenderer::init(Device& device) {
  HRESULT hr = m_perFrameBuffer.init(device, sizeof(CBPerFrame));
  if (FAILED(hr)) {
    return hr;
  }

  hr = m_perObjectBuffer.init(device, sizeof(CBPerObject));
  if (FAILED(hr)) {
    return hr;
  }

  hr = m_lightingDebugBuffer.init(device, sizeof(CBPerMaterial));
  if (FAILED(hr)) {
    return hr;
  }

  hr = m_lightingDebugBuffer.init(device, sizeof(DeferredLigthingDebugData));
  if (FAILED(hr)) {
    return hr;
  }

  hr = m_transparentDepthStencil.init(device,
    true,
    D3D11_DEPTH_WRITE_MASK_ZERO,
    D3D11_COMPARISON_LESS_EQUAL);
  if (FAILED(hr)) {
    return hr;
  }

  hr = m_disabledDepthStencil.init(device,
    false,
    D3D11_DEPTH_WRITE_MASK_ZERO,
    D3D11_COMPARISON_ALWAYS);
  if (FAILED(hr)) {
    return hr;
  }

  hr = m_shadowDepthStencil.init(device,
    true,
    D3D11_DEPTH_WRITE_MASK_ALL,
    D3D11_COMPARISON_LESS);
  if (FAILED(hr)) {
    return hr;
  }

  hr = createShadowResources(device);
  if (FAILED(hr)) {
    return hr;
  }

  hr = m_preShadowDebugPass.init(device, m_renderWidth, m_renderHeight);
  if (FAILED(hr)) {
    return hr;
  }

  hr = createGBufferResources(device, m_renderWidth, m_renderHeight);
  if (FAILED(hr)) {
    return hr;
  }

  hr = CreateLightResources(device);
  if (FAILED(hr)) {
    return hr;
  }

  hr = CreateFullScreenQuad(device);
  if (FAILED(hr)) {
    return hr;
  }

  hr = CreateBlenderStates(device);
  if (FAILED(hr)) {
    return hr;
  }

  return S_OK;
}

void
DeferredRenderer::resize(Device& device, unsigned int width, unsigned int height) {
  if (width < 64) width = 64;
  if (height < 64) height = 64;

  m_renderWidth = width;
  m_renderHeight = height;
  m_preShadowDebugPass.resize(device, width, height);
  createGBufferResources(device, width, height);
}

void 
DeferredRenderer::render(DeviceContext& deviceContext,
  const Camera& camera,
  RenderScene& scene,
  EditorViewportPass viewportPass) {
  buildQueues(scene, camera);
  updatePerFrame(camera, scene, deviceContext);

  renderSceneToTarget(deviceContext, scene, m_preShadowDebugPass, false);
  renderShadowPass(deviceContext);
  renderSceneToTarget(deviceContext, scene, viewportPass, true);
}

void
DeferredRenderer::destroy() {
  m_opaqueQueue.clear();
  m_transparentQueue.clear();

  SAFE_RELEASE(m_alphaBlendState);
  SAFE_RELEASE(m_opaqueBlendState);
  SAFE_RELEASE(m_additiveBlendState);
  SAFE_RELEASE(m_premultipliedBlendState);

  m_fullscreenIndexBuffer.destroy();
  m_fullscreenVertexBuffer.destroy();


  m_gBufferEmissiveAlphaRTV.destroy();
  m_gBufferEmissiveAlphaSRV.destroy();
  m_gBufferEmissiveAlphaTexture.destroy();
  m_gBufferWorldAoRTV.destroy();
  m_gBufferWorldAoSRV.destroy();
  m_gBufferWorldAoTexture.destroy();
  m_gBufferNormalRoughnessRTV.destroy();
  m_gBufferNormalRoughnessSRV.destroy();
  m_gBufferNormalRoughnessTexture.destroy();
  m_gBufferAlbedoMetallicRTV.destroy();
  m_gBufferAlbedoMetallicSRV.destroy();
  m_gBufferAlbedoMetallicTexture.destroy();

  m_fullscreenRasterizer.destroy();
  m_ligthingSampler.destroy();
  m_defferedLightingShader.destroy();
  m_gBufferShader.destroy();

  m_transparentDepthStencil.destroy();
  m_disabledDepthStencil.destroy();
  m_shadowDepthStencil.destroy();
  m_perMaterialBuffer.destroy();
  m_lightingDebugBuffer.destroy();
  m_perObjectBuffer.destroy();
  m_perFrameBuffer.destroy();

  m_shadowRasterizer.destroy();
  m_shadowShader.destroy();
  m_shadowDSV.destroy();
  m_shadowDepthSRV.destroy();
  m_shadowDepthTexture.destroy();
  m_preShadowDebugPass.destroy();

}

void
DeferredRenderer::buildQueues(RenderScene& scene, const Camera& camera) {
  (void)camera;
  m_opaqueQueue.clear();
  m_transparentQueue.clear();

  for (auto& object : scene.opaqueObjects) {
    m_opaqueQueue.push_back(&object);
  }

  for (auto& object : scene.transparentObjects) {
    m_transparentQueue.push_back(&object);
  }

std: : sort(m_opaqueQueue.begin(), m_opaqueQueue.end(),
  [](const RenderObject* lhs, const RenderObject* rhs) {
    if (lhs->materialInstance != rhs->materialInstance) {
      return lhs->materialInstance < rhs->materialInstance;
    }
    return lhs->distanceToCamera < rhs->distanceToCamera;
  });

std: : sort(m_transparentQueue.begin(), m_transparentQueue.end(),
  [](const RenderObject* lhs, const RenderObject* rhs) {
    return lhs->distanceToCamera > rhs->distanceToCamera;
  });
}
  