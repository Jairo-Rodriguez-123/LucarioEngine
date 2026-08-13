/** @file DepthStencilView.cpp */
#include "DepthStencilView.h"
#include "Device.h"
#include "DeviceContext.h"
#include "Texture.h"

DepthStencilView::DepthStencilView(DepthStencilView&& other) noexcept
  : m_depthStencilView(other.m_depthStencilView) {
  other.m_depthStencilView = nullptr;
}

DepthStencilView& DepthStencilView::operator=(DepthStencilView&& other) noexcept {
  if (this != &other) {
    destroy();
    m_depthStencilView = other.m_depthStencilView;
    other.m_depthStencilView = nullptr;
  }
  return *this;
}

DepthStencilView::~DepthStencilView() {
  destroy();
}

HRESULT DepthStencilView::init(Device& device, Texture& depthStencil, DXGI_FORMAT format) {
  if (!device.m_device) return E_POINTER;
  if (!depthStencil.m_texture) return E_POINTER;
  if (format == DXGI_FORMAT_UNKNOWN) return E_INVALIDARG;

  destroy();
  D3D11_TEXTURE2D_DESC texDesc{};
  depthStencil.m_texture->GetDesc(&texDesc);

  D3D11_DEPTH_STENCIL_VIEW_DESC desc{};
  desc.Format = format;
  desc.ViewDimension = texDesc.SampleDesc.Count > 1
    ? D3D11_DSV_DIMENSION_TEXTURE2DMS
    : D3D11_DSV_DIMENSION_TEXTURE2D;
  if (desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D) desc.Texture2D.MipSlice = 0;

  HRESULT hr = device.CreateDepthStencilView(depthStencil.m_texture, &desc, &m_depthStencilView);
  if (FAILED(hr)) ERROR("DepthStencilView", "init", "Failed to create depth stencil view");
  return hr;
}

HRESULT DepthStencilView::init(Device& device, Texture& depthStencil, DXGI_FORMAT format,
                               D3D11_DSV_DIMENSION viewDimension) {
  if (!device.m_device) return E_POINTER;
  if (!depthStencil.m_texture) return E_POINTER;
  if (format == DXGI_FORMAT_UNKNOWN) return E_INVALIDARG;

  destroy();
  D3D11_DEPTH_STENCIL_VIEW_DESC desc{};
  desc.Format = format;
  desc.ViewDimension = viewDimension;
  if (viewDimension == D3D11_DSV_DIMENSION_TEXTURE2D) desc.Texture2D.MipSlice = 0;

  HRESULT hr = device.CreateDepthStencilView(depthStencil.m_texture, &desc, &m_depthStencilView);
  if (FAILED(hr)) ERROR("DepthStencilView", "init", "Failed to create depth stencil view");
  return hr;
}

void DepthStencilView::render(DeviceContext& deviceContext) {
  if (!deviceContext.m_deviceContext || !m_depthStencilView) return;
  // Limpiar solo depth es valido incluso cuando el formato no contiene stencil.
  deviceContext.ClearDepthStencilView(m_depthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void DepthStencilView::destroy() { SAFE_RELEASE(m_depthStencilView); }
