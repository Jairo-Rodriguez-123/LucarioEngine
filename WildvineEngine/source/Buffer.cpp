/**
 * @file Buffer.cpp
 * @brief Implementa la logica de Buffer dentro del subsistema Core.
 * @ingroup core
 */
#include "Buffer.h"
#include "Device.h"
#include "DeviceContext.h"
#include <utility>
#include <limits>

namespace {
bool ComputeBufferByteWidth(size_t elementCount, unsigned int stride, unsigned int& outByteWidth) {
  if (elementCount == 0 || stride == 0) {
    return false;
  }
  if (elementCount > static_cast<size_t>(std::numeric_limits<unsigned int>::max() / stride)) {
    return false;
  }
  outByteWidth = static_cast<unsigned int>(elementCount) * stride;
  return outByteWidth != 0;
}
}

Buffer::Buffer(const Buffer& other)
  : m_buffer(other.m_buffer), m_stride(other.m_stride), m_offset(other.m_offset), m_bindFlag(other.m_bindFlag) {
  if (m_buffer) m_buffer->AddRef();
}

Buffer& Buffer::operator=(const Buffer& other) {
  if (this == &other) return *this;
  ID3D11Buffer* newBuffer = other.m_buffer;
  if (newBuffer) newBuffer->AddRef();
  SAFE_RELEASE(m_buffer);
  m_buffer = newBuffer;
  m_stride = other.m_stride;
  m_offset = other.m_offset;
  m_bindFlag = other.m_bindFlag;
  return *this;
}

Buffer::Buffer(Buffer&& other) noexcept
  : m_buffer(other.m_buffer), m_stride(other.m_stride), m_offset(other.m_offset), m_bindFlag(other.m_bindFlag) {
  other.m_buffer = nullptr;
  other.m_stride = 0;
  other.m_offset = 0;
  other.m_bindFlag = 0;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
  if (this == &other) return *this;
  destroy();
  m_buffer = other.m_buffer;
  m_stride = other.m_stride;
  m_offset = other.m_offset;
  m_bindFlag = other.m_bindFlag;
  other.m_buffer = nullptr;
  other.m_stride = 0;
  other.m_offset = 0;
  other.m_bindFlag = 0;
  return *this;
}

Buffer::~Buffer() { destroy(); }

HRESULT Buffer::init(Device& device, const MeshComponent& mesh, unsigned int bindFlag) {
  if (!device.m_device) return E_POINTER;
  if (bindFlag != D3D11_BIND_VERTEX_BUFFER && bindFlag != D3D11_BIND_INDEX_BUFFER) {
    ERROR("Buffer", "init", "bindFlag must be vertex or index buffer");
    return E_INVALIDARG;
  }
  if (bindFlag == D3D11_BIND_VERTEX_BUFFER && mesh.m_vertex.empty() && mesh.m_skyVertex.empty()) {
    ERROR("Buffer", "init", "Vertex data is empty");
    return E_INVALIDARG;
  }
  if (bindFlag == D3D11_BIND_INDEX_BUFFER && mesh.m_index.empty()) {
    ERROR("Buffer", "init", "Index data is empty");
    return E_INVALIDARG;
  }
  if (bindFlag == D3D11_BIND_INDEX_BUFFER) {
    const size_t vertexCount = !mesh.m_vertex.empty() ? mesh.m_vertex.size() : mesh.m_skyVertex.size();
    if (vertexCount == 0) {
      ERROR("Buffer", "init", "Index buffer has no corresponding vertex data");
      return E_INVALIDARG;
    }
    for (const unsigned int index : mesh.m_index) {
      if (index >= vertexCount) {
        ERROR("Buffer", "init", "Index references a vertex outside the mesh");
        return E_INVALIDARG;
      }
    }
  }

  destroy();
  D3D11_BUFFER_DESC desc{};
  D3D11_SUBRESOURCE_DATA data{};
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = bindFlag;
  m_bindFlag = bindFlag;

  if (bindFlag == D3D11_BIND_VERTEX_BUFFER) {
    if (!mesh.m_skyVertex.empty() && mesh.m_vertex.empty()) {
      m_stride = sizeof(SkyboxVertex);
      if (!ComputeBufferByteWidth(mesh.m_skyVertex.size(), m_stride, desc.ByteWidth)) {
        ERROR("Buffer", "init", "Skybox vertex buffer is too large for D3D11");
        destroy();
        return E_INVALIDARG;
      }
      data.pSysMem = mesh.m_skyVertex.data();
    } else {
      m_stride = sizeof(SimpleVertex);
      if (!ComputeBufferByteWidth(mesh.m_vertex.size(), m_stride, desc.ByteWidth)) {
        ERROR("Buffer", "init", "Vertex buffer is too large for D3D11");
        destroy();
        return E_INVALIDARG;
      }
      data.pSysMem = mesh.m_vertex.data();
    }
  } else {
    m_stride = sizeof(unsigned int);
    if (!ComputeBufferByteWidth(mesh.m_index.size(), m_stride, desc.ByteWidth)) {
      ERROR("Buffer", "init", "Index buffer is too large for D3D11");
      destroy();
      return E_INVALIDARG;
    }
    data.pSysMem = mesh.m_index.data();
  }

  HRESULT hr = createBuffer(device, desc, &data);
  if (FAILED(hr)) {
    m_stride = m_offset = m_bindFlag = 0;
  }
  return hr;
}

HRESULT Buffer::init(Device& device, unsigned int ByteWidth) {
  if (!device.m_device) return E_POINTER;
  if (ByteWidth == 0) return E_INVALIDARG;

  destroy();
  // D3D11 requires constant buffers to be a multiple of 16 bytes. Do not
  // silently round the allocation: UpdateSubresource copies the whole buffer
  // and rounding could make it read beyond the caller's source structure.
  if ((ByteWidth & 15u) != 0u) {
    ERROR("Buffer", "init", "Constant buffer size must be a multiple of 16 bytes");
    return E_INVALIDARG;
  }

  D3D11_BUFFER_DESC desc{};
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.ByteWidth = ByteWidth;
  desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
  m_stride = ByteWidth;
  m_bindFlag = desc.BindFlags;
  const HRESULT hr = createBuffer(device, desc, nullptr);
  if (FAILED(hr)) {
    m_stride = m_offset = m_bindFlag = 0;
  }
  return hr;
}

void Buffer::update(DeviceContext& deviceContext, ID3D11Resource* pDstResource,
                    unsigned int DstSubresource, const D3D11_BOX* pDstBox,
                    const void* pSrcData, unsigned int SrcRowPitch,
                    unsigned int SrcDepthPitch) {
  if (!deviceContext.m_deviceContext) return;
  if (!pSrcData) return;
  ID3D11Resource* destination = pDstResource ? pDstResource : m_buffer;
  if (!destination) return;
  deviceContext.UpdateSubresource(destination, DstSubresource, pDstBox, pSrcData,
                                  SrcRowPitch, SrcDepthPitch);
}

void Buffer::render(DeviceContext& deviceContext, unsigned int StartSlot,
                    unsigned int NumBuffers, bool setPixelShader, DXGI_FORMAT format) {
  if (!deviceContext.m_deviceContext || !m_buffer || NumBuffers == 0) return;
  // Esta clase representa un solo buffer; pedir mas de uno haria que D3D leyera memoria adyacente.
  if (NumBuffers != 1) NumBuffers = 1;

  switch (m_bindFlag) {
    case D3D11_BIND_VERTEX_BUFFER:
      deviceContext.IASetVertexBuffers(StartSlot, 1, &m_buffer, &m_stride, &m_offset);
      break;
    case D3D11_BIND_CONSTANT_BUFFER:
      deviceContext.VSSetConstantBuffers(StartSlot, 1, &m_buffer);
      if (setPixelShader) deviceContext.PSSetConstantBuffers(StartSlot, 1, &m_buffer);
      break;
    case D3D11_BIND_INDEX_BUFFER:
      if (format == DXGI_FORMAT_UNKNOWN) format = DXGI_FORMAT_R32_UINT;
      deviceContext.IASetIndexBuffer(m_buffer, format, m_offset);
      break;
    default:
      ERROR("Buffer", "render", "Unsupported BindFlag");
      break;
  }
}

void Buffer::destroy() {
  SAFE_RELEASE(m_buffer);
  m_stride = 0;
  m_offset = 0;
  m_bindFlag = 0;
}

HRESULT Buffer::createBuffer(Device& device, D3D11_BUFFER_DESC& desc,
                             D3D11_SUBRESOURCE_DATA* initData) {
  if (!device.m_device) return E_POINTER;
  if (desc.ByteWidth == 0) return E_INVALIDARG;
  SAFE_RELEASE(m_buffer);
  HRESULT hr = device.CreateBuffer(&desc, initData, &m_buffer);
  if (FAILED(hr)) ERROR("Buffer", "createBuffer", "Failed to create buffer");
  return hr;
}
