/** @file RenderTargetView.cpp */
#include "RenderTargetView.h"
#include "Device.h"
#include "Texture.h"
#include "DeviceContext.h"
#include "DepthStencilView.h"

RenderTargetView::RenderTargetView(RenderTargetView&& other) noexcept
  : m_renderTargetView(other.m_renderTargetView) {
  other.m_renderTargetView = nullptr;
}

RenderTargetView& RenderTargetView::operator=(RenderTargetView&& other) noexcept {
  if (this != &other) {
    destroy();
    m_renderTargetView = other.m_renderTargetView;
    other.m_renderTargetView = nullptr;
  }
  return *this;
}

RenderTargetView::~RenderTargetView() {
  destroy();
}

HRESULT RenderTargetView::init(Device& device, Texture& backBuffer, DXGI_FORMAT Format) {
  if (!device.m_device) return E_POINTER;
  if (!backBuffer.m_texture) return E_POINTER;
  if (Format == DXGI_FORMAT_UNKNOWN) return E_INVALIDARG;

  destroy();
  D3D11_TEXTURE2D_DESC texDesc{};
  backBuffer.m_texture->GetDesc(&texDesc);

  D3D11_RENDER_TARGET_VIEW_DESC desc{};
  desc.Format = Format;
  desc.ViewDimension = texDesc.SampleDesc.Count > 1
    ? D3D11_RTV_DIMENSION_TEXTURE2DMS
    : D3D11_RTV_DIMENSION_TEXTURE2D;
  if (desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D) desc.Texture2D.MipSlice = 0;

  HRESULT hr = device.CreateRenderTargetView(backBuffer.m_texture, &desc, &m_renderTargetView);
  if (FAILED(hr)) ERROR("RenderTargetView", "init", "Failed to create render target view");
  return hr;
}

HRESULT RenderTargetView::init(Device& device, Texture& inTex,
                               D3D11_RTV_DIMENSION ViewDimension, DXGI_FORMAT Format) {
  if (!device.m_device) return E_POINTER;
  if (!inTex.m_texture) return E_POINTER;
  if (Format == DXGI_FORMAT_UNKNOWN) return E_INVALIDARG;

  destroy();
  D3D11_RENDER_TARGET_VIEW_DESC desc{};
  desc.Format = Format;
  desc.ViewDimension = ViewDimension;
  if (ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D) desc.Texture2D.MipSlice = 0;

  HRESULT hr = device.CreateRenderTargetView(inTex.m_texture, &desc, &m_renderTargetView);
  if (FAILED(hr)) ERROR("RenderTargetView", "init", "Failed to create render target view");
  return hr;
}

void RenderTargetView::update() {}

void RenderTargetView::render(DeviceContext& deviceContext, DepthStencilView& depthStencilView,
                              unsigned int numViews, const float ClearColor[4]) {
  if (!deviceContext.m_deviceContext || !m_renderTargetView || !ClearColor) return;
  // Esta clase encapsula exactamente un RTV.
  (void)numViews;
  deviceContext.ClearRenderTargetView(m_renderTargetView, ClearColor);
  deviceContext.OMSetRenderTargets(1, &m_renderTargetView, depthStencilView.m_depthStencilView);
}

void RenderTargetView::render(DeviceContext& deviceContext, unsigned int numViews) {
  if (!deviceContext.m_deviceContext || !m_renderTargetView) return;
  (void)numViews;
  deviceContext.OMSetRenderTargets(1, &m_renderTargetView, nullptr);
}

void RenderTargetView::destroy() { SAFE_RELEASE(m_renderTargetView); }
