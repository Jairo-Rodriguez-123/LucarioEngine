/**
 * @file DeviceContext.cpp
 * @brief Implementa la logica de DeviceContext dentro del subsistema Core.
 * @ingroup core
 */
#include "DeviceContext.h"

namespace {
bool EnsureContext(ID3D11DeviceContext* context, const char* method) {
  if (context) return true;
  ERROR("DeviceContext", method, "m_deviceContext is nullptr");
  return false;
}
}

void DeviceContext::init() {}
void DeviceContext::update() {}
void DeviceContext::render() {}

void DeviceContext::destroy() {
  SAFE_RELEASE(m_deviceContext);
}

void DeviceContext::RSSetViewports(unsigned int NumViewports, const D3D11_VIEWPORT* pViewports) {
  if (!EnsureContext(m_deviceContext, "RSSetViewports")) return;
  if (NumViewports > 0 && !pViewports) {
    ERROR("DeviceContext", "RSSetViewports", "pViewports is nullptr while NumViewports > 0");
    return;
  }
  m_deviceContext->RSSetViewports(NumViewports, pViewports);
}

void DeviceContext::PSSetShaderResources(unsigned int StartSlot, unsigned int NumViews,
                                         ID3D11ShaderResourceView* const* ppShaderResourceViews) {
  if (!EnsureContext(m_deviceContext, "PSSetShaderResources")) return;
  if (NumViews > 0 && !ppShaderResourceViews) {
    ERROR("DeviceContext", "PSSetShaderResources", "ppShaderResourceViews is nullptr while NumViews > 0");
    return;
  }
  m_deviceContext->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

void DeviceContext::IASetInputLayout(ID3D11InputLayout* pInputLayout) {
  if (!EnsureContext(m_deviceContext, "IASetInputLayout")) return;
  // nullptr es valido y desactiva el input layout actual.
  m_deviceContext->IASetInputLayout(pInputLayout);
}

void DeviceContext::VSSetShader(ID3D11VertexShader* pVertexShader,
                                ID3D11ClassInstance* const* ppClassInstances,
                                unsigned int NumClassInstances) {
  if (!EnsureContext(m_deviceContext, "VSSetShader")) return;
  m_deviceContext->VSSetShader(pVertexShader, ppClassInstances, NumClassInstances);
}

void DeviceContext::PSSetShader(ID3D11PixelShader* pPixelShader,
                                ID3D11ClassInstance* const* ppClassInstances,
                                unsigned int NumClassInstances) {
  if (!EnsureContext(m_deviceContext, "PSSetShader")) return;
  m_deviceContext->PSSetShader(pPixelShader, ppClassInstances, NumClassInstances);
}

void DeviceContext::UpdateSubresource(ID3D11Resource* pDstResource,
                                      unsigned int DstSubresource,
                                      const D3D11_BOX* pDstBox,
                                      const void* pSrcData,
                                      unsigned int SrcRowPitch,
                                      unsigned int SrcDepthPitch) {
  if (!EnsureContext(m_deviceContext, "UpdateSubresource")) return;
  if (!pDstResource || !pSrcData) {
    ERROR("DeviceContext", "UpdateSubresource", "pDstResource or pSrcData is nullptr");
    return;
  }
  m_deviceContext->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData,
                                     SrcRowPitch, SrcDepthPitch);
}

void DeviceContext::IASetVertexBuffers(unsigned int StartSlot, unsigned int NumBuffers,
                                       ID3D11Buffer* const* ppVertexBuffers,
                                       const unsigned int* pStrides,
                                       const unsigned int* pOffsets) {
  if (!EnsureContext(m_deviceContext, "IASetVertexBuffers")) return;
  if (NumBuffers > 0 && (!ppVertexBuffers || !pStrides || !pOffsets)) {
    ERROR("DeviceContext", "IASetVertexBuffers", "Buffer, stride or offset array is nullptr");
    return;
  }
  m_deviceContext->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}

void DeviceContext::IASetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format,
                                     unsigned int Offset) {
  if (!EnsureContext(m_deviceContext, "IASetIndexBuffer")) return;
  // nullptr es valido para desasignar el index buffer.
  m_deviceContext->IASetIndexBuffer(pIndexBuffer, Format, Offset);
}

void DeviceContext::PSSetSamplers(unsigned int StartSlot, unsigned int NumSamplers,
                                  ID3D11SamplerState* const* ppSamplers) {
  if (!EnsureContext(m_deviceContext, "PSSetSamplers")) return;
  if (NumSamplers > 0 && !ppSamplers) {
    ERROR("DeviceContext", "PSSetSamplers", "ppSamplers is nullptr while NumSamplers > 0");
    return;
  }
  m_deviceContext->PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

void DeviceContext::RSSetState(ID3D11RasterizerState* pRasterizerState) {
  if (!EnsureContext(m_deviceContext, "RSSetState")) return;
  // nullptr restaura el rasterizer state por defecto.
  m_deviceContext->RSSetState(pRasterizerState);
}

void DeviceContext::OMSetBlendState(ID3D11BlendState* pBlendState,
                                    const float BlendFactor[4], unsigned int SampleMask) {
  if (!EnsureContext(m_deviceContext, "OMSetBlendState")) return;
  // nullptr restaura el blend state por defecto.
  m_deviceContext->OMSetBlendState(pBlendState, BlendFactor, SampleMask);
}

void DeviceContext::OMSetRenderTargets(unsigned int NumViews,
                                       ID3D11RenderTargetView* const* ppRenderTargetViews,
                                       ID3D11DepthStencilView* pDepthStencilView) {
  if (!EnsureContext(m_deviceContext, "OMSetRenderTargets")) return;
  if (NumViews > 0 && !ppRenderTargetViews) {
    ERROR("DeviceContext", "OMSetRenderTargets", "ppRenderTargetViews is nullptr while NumViews > 0");
    return;
  }
  // (0, nullptr, nullptr) es valido y desasigna todos los targets.
  m_deviceContext->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);
}

void DeviceContext::IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) {
  if (!EnsureContext(m_deviceContext, "IASetPrimitiveTopology")) return;
  if (Topology == D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED) {
    ERROR("DeviceContext", "IASetPrimitiveTopology", "Topology is undefined");
    return;
  }
  m_deviceContext->IASetPrimitiveTopology(Topology);
}

void DeviceContext::ClearRenderTargetView(ID3D11RenderTargetView* pRenderTargetView,
                                          const float ColorRGBA[4]) {
  if (!EnsureContext(m_deviceContext, "ClearRenderTargetView")) return;
  if (!pRenderTargetView || !ColorRGBA) {
    ERROR("DeviceContext", "ClearRenderTargetView", "Render target or color is nullptr");
    return;
  }
  m_deviceContext->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
}

void DeviceContext::ClearDepthStencilView(ID3D11DepthStencilView* pDepthStencilView,
                                          unsigned int ClearFlags, float Depth, UINT8 Stencil) {
  if (!EnsureContext(m_deviceContext, "ClearDepthStencilView")) return;
  if (!pDepthStencilView) {
    ERROR("DeviceContext", "ClearDepthStencilView", "pDepthStencilView is nullptr");
    return;
  }
  if ((ClearFlags & (D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL)) == 0) {
    ERROR("DeviceContext", "ClearDepthStencilView", "ClearFlags does not contain depth or stencil");
    return;
  }
  m_deviceContext->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
}

void DeviceContext::VSSetConstantBuffers(unsigned int StartSlot, unsigned int NumBuffers,
                                         ID3D11Buffer* const* ppConstantBuffers) {
  if (!EnsureContext(m_deviceContext, "VSSetConstantBuffers")) return;
  if (NumBuffers > 0 && !ppConstantBuffers) {
    ERROR("DeviceContext", "VSSetConstantBuffers", "ppConstantBuffers is nullptr while NumBuffers > 0");
    return;
  }
  m_deviceContext->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

void DeviceContext::PSSetConstantBuffers(unsigned int StartSlot, unsigned int NumBuffers,
                                         ID3D11Buffer* const* ppConstantBuffers) {
  if (!EnsureContext(m_deviceContext, "PSSetConstantBuffers")) return;
  if (NumBuffers > 0 && !ppConstantBuffers) {
    ERROR("DeviceContext", "PSSetConstantBuffers", "ppConstantBuffers is nullptr while NumBuffers > 0");
    return;
  }
  m_deviceContext->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

void DeviceContext::DrawIndexed(unsigned int IndexCount, unsigned int StartIndexLocation,
                                int BaseVertexLocation) {
  if (!EnsureContext(m_deviceContext, "DrawIndexed")) return;
  if (IndexCount == 0) return; // Dibujo vacio: no es un error.
  m_deviceContext->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
}
