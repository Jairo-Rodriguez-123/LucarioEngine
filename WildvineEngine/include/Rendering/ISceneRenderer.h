/**
*@file ISceneRenderer.h
*@biref Declara una interfaz comun para los renderers de escena.
*@inroup rendering
*/
#pragma once

#include"Prerequisites.h"

class Device;
class DeviceContext;
class Camera;
class RenderScene;
class EditorViewportPass;

enum class
  SceneRendererType {
  Forward =0,
  Deferred
};

/**
*@class ISceneRenderer
* @brief contrato base para cualquier renderer consumido por el pipeline principal.
*/
class
  ISceneRenderer {
public:
  virtual ~ISceneRenderer() = default;

  virtual HRESULT init(Device& device) = 0;
  virtual void resize(Device& device, unsigned int width, unsigned int height) = 0;
  virtual void render(DeviceContext& deviceContext,
    const Camera& camera,
    const RenderScene& scene,
    EditorViewportPass viewportPass) = 0;
  virtual void destroy() = 0;

  virtual ID3D11ShaderResourceView* getShadowMapSRV() const { return nullptr; }
  virtual ID3D11ShaderResourceView* getPreShadowSRV() const { return nullptr; }
  virtual ID3D11ShaderResourceView* getGBufferAlbedoMetallicSRV() const { return nullptr; }
  virtual ID3D11ShaderResourceView* getGBufferNormalRoughnessSRV() const { return nullptr; }
  virtual ID3D11ShaderResourceView* getGBufferWorldAoSVR() const { return nullptr; }
  virtual ID3D11ShaderResourceView* getGBufferEmissiveAlphaSRV() const { return nullptr; }
  virtual void setShadowFactorDebugEnabled(bool enabled) { (void)enabled; }
  virtual void setDeferredDebugViewMode(int mode) { (void)mode; }
  virtual const char* getDebugName() const = 0;
};