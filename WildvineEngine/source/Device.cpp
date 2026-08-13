/**
 * @file Device.cpp
 * @brief Implementa la logica de Device dentro del subsistema Core.
 * @ingroup core
 */
#include "Device.h"

void Device::init() {}
void Device::update() {}
void Device::render() {}

void Device::destroy() {
  SAFE_RELEASE(m_device);
}

HRESULT Device::CreateRenderTargetView(ID3D11Resource* pResource,
                                       const D3D11_RENDER_TARGET_VIEW_DESC* pDesc,
                                       ID3D11RenderTargetView** ppRTView) {
  if (!m_device) return E_POINTER;
  if (!pResource) return E_INVALIDARG;
  if (!ppRTView) return E_POINTER;
  *ppRTView = nullptr;
  HRESULT hr = m_device->CreateRenderTargetView(pResource, pDesc, ppRTView);
  if (FAILED(hr)) ERROR("Device", "CreateRenderTargetView", "Failed to create render target view");
  return hr;
}

HRESULT Device::CreateTexture2D(const D3D11_TEXTURE2D_DESC* pDesc,
                                const D3D11_SUBRESOURCE_DATA* pInitialData,
                                ID3D11Texture2D** ppTexture2D) {
  if (!m_device) return E_POINTER;
  if (!pDesc) return E_INVALIDARG;
  if (!ppTexture2D) return E_POINTER;
  *ppTexture2D = nullptr;
  HRESULT hr = m_device->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
  if (FAILED(hr)) ERROR("Device", "CreateTexture2D", "Failed to create texture2D");
  return hr;
}

HRESULT Device::CreateDepthStencilView(ID3D11Resource* pResource,
                                       const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc,
                                       ID3D11DepthStencilView** ppDepthStencilView) {
  if (!m_device) return E_POINTER;
  if (!pResource) return E_INVALIDARG;
  if (!ppDepthStencilView) return E_POINTER;
  *ppDepthStencilView = nullptr;
  HRESULT hr = m_device->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
  if (FAILED(hr)) ERROR("Device", "CreateDepthStencilView", "Failed to create depth stencil view");
  return hr;
}

HRESULT Device::CreateVertexShader(const void* pShaderBytecode, unsigned int BytecodeLength,
                                   ID3D11ClassLinkage* pClassLinkage,
                                   ID3D11VertexShader** ppVertexShader) {
  if (!m_device) return E_POINTER;
  if (!pShaderBytecode || BytecodeLength == 0) return E_INVALIDARG;
  if (!ppVertexShader) return E_POINTER;
  *ppVertexShader = nullptr;
  HRESULT hr = m_device->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
  if (FAILED(hr)) ERROR("Device", "CreateVertexShader", "Failed to create vertex shader");
  return hr;
}

HRESULT Device::CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs,
                                  unsigned int NumElements,
                                  const void* pShaderBytecodeWithInputSignature,
                                  unsigned int BytecodeLength,
                                  ID3D11InputLayout** ppInputLayout) {
  if (!m_device) return E_POINTER;
  if (!pInputElementDescs || NumElements == 0 || !pShaderBytecodeWithInputSignature || BytecodeLength == 0)
    return E_INVALIDARG;
  if (!ppInputLayout) return E_POINTER;
  *ppInputLayout = nullptr;
  HRESULT hr = m_device->CreateInputLayout(pInputElementDescs, NumElements,
                                           pShaderBytecodeWithInputSignature, BytecodeLength,
                                           ppInputLayout);
  if (FAILED(hr)) ERROR("Device", "CreateInputLayout", "Failed to create input layout");
  return hr;
}

HRESULT Device::CreatePixelShader(const void* pShaderBytecode, unsigned int BytecodeLength,
                                  ID3D11ClassLinkage* pClassLinkage,
                                  ID3D11PixelShader** ppPixelShader) {
  if (!m_device) return E_POINTER;
  if (!pShaderBytecode || BytecodeLength == 0) return E_INVALIDARG;
  if (!ppPixelShader) return E_POINTER;
  *ppPixelShader = nullptr;
  HRESULT hr = m_device->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
  if (FAILED(hr)) ERROR("Device", "CreatePixelShader", "Failed to create pixel shader");
  return hr;
}

HRESULT Device::CreateSamplerState(const D3D11_SAMPLER_DESC* pSamplerDesc,
                                   ID3D11SamplerState** ppSamplerState) {
  if (!m_device) return E_POINTER;
  if (!pSamplerDesc) return E_INVALIDARG;
  if (!ppSamplerState) return E_POINTER;
  *ppSamplerState = nullptr;
  HRESULT hr = m_device->CreateSamplerState(pSamplerDesc, ppSamplerState);
  if (FAILED(hr)) ERROR("Device", "CreateSamplerState", "Failed to create sampler state");
  return hr;
}

HRESULT Device::CreateBuffer(const D3D11_BUFFER_DESC* pDesc,
                             const D3D11_SUBRESOURCE_DATA* pInitialData,
                             ID3D11Buffer** ppBuffer) {
  if (!m_device) return E_POINTER;
  if (!pDesc || pDesc->ByteWidth == 0) return E_INVALIDARG;
  if (!ppBuffer) return E_POINTER;
  *ppBuffer = nullptr;
  HRESULT hr = m_device->CreateBuffer(pDesc, pInitialData, ppBuffer);
  if (FAILED(hr)) ERROR("Device", "CreateBuffer", "Failed to create buffer");
  return hr;
}
