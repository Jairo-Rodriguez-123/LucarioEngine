#pragma once
#include"Prerequisites.h"

class Device;
/*
  *  @brief Forward declaration for the graphics device abstraction.
*/
class DeviceContext;
/*
  *  @brief Forward declaration for the device context used for rendering commands.
*/
class Camera;
/*
  *  @brief Forward declaration for the camera representation used during rendering.
*/
class RenderScene;
/*
  *  @brief Forward declaration for the scene container holding renderable objects.
*/
class EditorViewportPass;
/*
  *  @brief Forward declaration for the editor viewport pass enumeration or type.
*/
enum class
  SceneRendererType {
  /*
    *  @brief Enumerates the available scene renderer implementations.
  */
  Forward =0,
  /*
    *  @brief Forward rendering pipeline (single pass lighting).
  */
  Deferred
  /*
    *  @brief Deferred rendering pipeline (multiple G-buffer passes).
  */
};

class
  ISceneRenderer {
public:
  /*
    *  @brief Virtual destructor to allow proper cleanup in derived renderers.
  */
  virtual ~ISceneRenderer() = default;

  /*
    *  @brief Initialize renderer resources using the provided device.
    *
    *  @param device Reference to the Device used to create GPU resources.
    *  @return HRESULT indicating success or failure of initialization.
  */
  virtual HRESULT init(Device& device) = 0;
  /*
    *  @brief Resize any render targets or resources to match new dimensions.
    *
    *  @param device Reference to the Device for resource updates.
    *  @param width New width in pixels.
    *  @param height New height in pixels.
  */
  virtual void resize(Device& device, unsigned int width, unsigned int height) = 0;
  /*
    *  @brief Render the scene from the provided camera into the current render targets.
    *
    *  @param deviceContext DeviceContext used to record rendering commands.
    *  @param camera Camera describing view and projection.
    *  @param scene Scene containing visible objects, lights and other data.
    *  @param viewportPass Identifier or flag specifying the editor viewport pass.
  */
  virtual void render(DeviceContext& deviceContext,
    const Camera& camera,
    const RenderScene& scene,
    EditorViewportPass viewportPass) = 0;
  /*
    *  @brief Release all GPU and CPU resources owned by the renderer.
  */
  virtual void destroy() = 0;

  /*
    *  @brief Get shader resource view for the main shadow map, if available.
    *  @return Pointer to ID3D11ShaderResourceView or nullptr when unsupported.
  */
  virtual ID3D11ShaderResourceView* getShadowMapSRV() const { return nullptr; }
  /*
    *  @brief Get shader resource view for the pre-shadow buffer, if available.
    *  @return Pointer to ID3D11ShaderResourceView or nullptr when unsupported.
  */
  virtual ID3D11ShaderResourceView* getPreShadowSRV() const { return nullptr; }
  /*
    *  @brief Get shader resource view for G-Buffer albedo + metallic channel.
    *  @return Pointer to ID3D11ShaderResourceView or nullptr when unsupported.
  */
  virtual ID3D11ShaderResourceView* getGBufferAlbedoMetallicSRV() const { return nullptr; }
  /*
    *  @brief Get shader resource view for G-Buffer normal + roughness channel.
    *  @return Pointer to ID3D11ShaderResourceView or nullptr when unsupported.
  */
  virtual ID3D11ShaderResourceView* getGBufferNormalRoughnessSRV() const { return nullptr; }
  /*
    *  @brief Get shader resource view for G-Buffer world-space AO (ambient occlusion).
    *  @return Pointer to ID3D11ShaderResourceView or nullptr when unsupported.
  */
  virtual ID3D11ShaderResourceView* getGBufferWorldAoSVR() const { return nullptr; }
  /*
    *  @brief Get shader resource view for G-Buffer emissive + alpha channel.
    *  @return Pointer to ID3D11ShaderResourceView or nullptr when unsupported.
  */
  virtual ID3D11ShaderResourceView* getGBufferEmissiveAlphaSRV() const { return nullptr; }
  /*
    *  @brief Enable or disable debug visualization for shadow factor.
    *
    *  @param enabled True to enable debug view, false to disable.
  */
  virtual void setShadowFactorDebugEnabled(bool enabled) { (void)enabled; }
  /*
    *  @brief Select a debug visualization mode for deferred rendering.
    *
    *  @param mode Integer identifying the debug view mode.
  */
  virtual void setDeferredDebugViewMode(int mode) { (void)mode; }
  /*
    *  @brief Retrieve a human-readable debug name for the renderer implementation.
    *
    *  @return Null-terminated C-string with the debug name.
  */
  virtual const char* getDebugName() const = 0;
};