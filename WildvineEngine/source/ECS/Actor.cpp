/**
 * @file Actor.cpp
 * @brief Implementa la logica de Actor dentro del subsistema ECS.
 * @ingroup ecs
 */
#include "ECS/Actor.h"
#include "MeshComponent.h"
#include "Device.h"
#include "DeviceContext.h"
#include <limits>

Actor::Actor() {
	auto transform = EU::MakeShared<Transform>();
	transform->init();
	addComponent(transform);
	auto meshComponent = EU::MakeShared<MeshComponent>();
	meshComponent->init();
	addComponent(meshComponent);
}

Actor::Actor(Device& device) {
	// Setup Default Components
	EU::TSharedPointer<Transform> transform = EU::MakeShared<Transform>();
	transform->init();
	addComponent(transform);
	EU::TSharedPointer<MeshComponent> meshComponent = EU::MakeShared<MeshComponent>();
	meshComponent->init();
	addComponent(meshComponent);

	HRESULT hr;
	std::string classNameType = "Actor -> " + m_name;
	hr = m_modelBuffer.init(device, sizeof(CBChangesEveryFrame));
	if (FAILED(hr)) {
		ERROR("Actor", classNameType.c_str(), "Failed to create new CBChangesEveryFrame");
	}

	// Awake
	awake();

	hr = m_sampler.init(device);
	if (FAILED(hr)) {
		ERROR("Actor", classNameType.c_str(), "Failed to create new SamplerState");
	}

	//hr = m_rasterizer.init(device);
	//if (FAILED(hr)) {
	//	ERROR("Actor", classNameType.c_str(), "Failed to create new Rasterizer");
	//}

	//hr = m_blendstate.init(device);
	//if (FAILED(hr)) {
	//	ERROR("Actor", classNameType.c_str(), "Failed to create new BlendState");
	//}

	//hr = m_shaderShadow.CreateShader(device, PIXEL_SHADER, "HybridEngine.fx");
	//
	//if (FAILED(hr)) {
	//	ERROR("Main", "InitDevice",
	//		("Failed to initialize Shadow Shader. HRESULT: " + std::to_string(hr)).c_str());
	//}
	//
	//hr = m_shaderBuffer.init(device, sizeof(CBChangesEveryFrame));
	//if (FAILED(hr)) {
	//	ERROR("Main", "InitDevice",
	//		("Failed to initialize Shadow Buffer. HRESULT: " + std::to_string(hr)).c_str());
	//
	//}
	//
	//hr = m_shadowBlendState.init(device);
	//if (FAILED(hr)) {
	//	ERROR("Main", "InitDevice",
	//		("Failed to initialize Shadow Blend State. HRESULT: " + std::to_string(hr)).c_str());
	//
	//}

	//hr = m_shadowDepthStencilState.init(device, true, false);
	//
	//if (FAILED(hr)) {
	//	ERROR("Main", "InitDevice",
	//		("Failed to initialize Depth Stencil State. HRESULT: " + std::to_string(hr)).c_str());
	//
	//}
	//
	//m_LightPos = XMFLOAT4(2.0f, 4.0f, -2.0f, 1.0f);
}

void
Actor::update(float deltaTime, DeviceContext& deviceContext) {
  (void)deviceContext;
	// Update all components
	for (auto& component : m_components) {
		if (component) {
			component->update(deltaTime);
		}
	}

}

void
Actor::syncModelBuffer(DeviceContext& deviceContext) {
	if (!deviceContext.m_deviceContext || !m_modelBuffer.m_buffer) {
		return;
	}
	auto transform = getComponent<Transform>();
	if (!transform) {
		return;
	}
	m_model.mWorld = XMMatrixTranspose(transform->worldMatrix);
	m_model.vMeshColor = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	m_modelBuffer.update(deviceContext, nullptr, 0, nullptr, &m_model, 0, 0);
}

void
Actor::render(DeviceContext& deviceContext) {
	if (!deviceContext.m_deviceContext) return;
	syncModelBuffer(deviceContext);
	m_sampler.render(deviceContext, 0, 1);
	deviceContext.IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	const size_t drawCount = std::min(m_meshes.size(), std::min(m_vertexBuffers.size(), m_indexBuffers.size()));
	for (size_t i = 0; i < drawCount; ++i) {
		m_vertexBuffers[i].render(deviceContext, 0, 1);
		m_indexBuffers[i].render(deviceContext, 0, 1, false, DXGI_FORMAT_R32_UINT);
		m_modelBuffer.render(deviceContext, 1, 1, true);

		const size_t requestedSlots = std::max(m_lastTextureSlotCount, m_textures.size());
		const size_t clearSlots = std::min(requestedSlots, static_cast<size_t>(D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT));
		if (clearSlots > 0) {
			std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> nullSRVs{};
			deviceContext.PSSetShaderResources(0, static_cast<unsigned int>(clearSlots), nullSRVs.data());
		}
		const size_t textureCount = std::min(m_textures.size(), static_cast<size_t>(D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT));
		for (size_t k = 0; k < textureCount; ++k) {
			m_textures[k].render(deviceContext, static_cast<unsigned int>(k), 1);
		}
		m_lastTextureSlotCount = textureCount;
		deviceContext.DrawIndexed(static_cast<unsigned int>(m_meshes[i].m_index.size()), 0, 0);
	}
}

void
Actor::renderForSkybox(DeviceContext& deviceContext) {
	if (!deviceContext.m_deviceContext) return;
	deviceContext.IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	const size_t drawCount = std::min(m_meshes.size(), std::min(m_vertexBuffers.size(), m_indexBuffers.size()));
	for (size_t i = 0; i < drawCount; ++i) {
		m_vertexBuffers[i].render(deviceContext, 0, 1);
		m_indexBuffers[i].render(deviceContext, 0, 1, false, DXGI_FORMAT_R32_UINT);
		deviceContext.DrawIndexed(static_cast<unsigned int>(m_meshes[i].m_index.size()), 0, 0);
	}
}

void
Actor::renderShadow(DeviceContext& deviceContext) {
	if (!castShadow || !deviceContext.m_deviceContext) return;
	syncModelBuffer(deviceContext);
	deviceContext.IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	const size_t drawCount = std::min(m_meshes.size(), std::min(m_vertexBuffers.size(), m_indexBuffers.size()));
	for (size_t i = 0; i < drawCount; ++i) {
		m_vertexBuffers[i].render(deviceContext, 0, 1);
		m_indexBuffers[i].render(deviceContext, 0, 1, false, DXGI_FORMAT_R32_UINT);
		m_modelBuffer.render(deviceContext, 1, 1, false);
		deviceContext.DrawIndexed(static_cast<unsigned int>(m_meshes[i].m_index.size()), 0, 0);
	}
}



void
Actor::destroy() {
	for (auto& vertexBuffer : m_vertexBuffers) {
		vertexBuffer.destroy();
	}

	for (auto& indexBuffer : m_indexBuffers) {
		indexBuffer.destroy();
	}

	for (auto& tex : m_textures) {
		tex.destroy();
	}
	m_modelBuffer.destroy();
	m_shaderBuffer.destroy();
	m_shaderShadow.destroy();
	m_shadowDepthStencilState.destroy();
	m_vertexBuffers.clear();
	m_indexBuffers.clear();
	m_textures.clear();
	m_lastTextureSlotCount = 0;
	m_meshes.clear();

	//m_rasterizer.destroy();
	//m_blendstate.destroy();
	m_sampler.destroy();
}

void
Actor::setMesh(Device& device, std::vector<MeshComponent> meshes) {
	for (auto& b : m_vertexBuffers) b.destroy();
	for (auto& b : m_indexBuffers) b.destroy();
	m_vertexBuffers.clear();
	m_indexBuffers.clear();
	m_meshes.clear();

	for (auto& mesh : meshes) {
		if ((mesh.m_vertex.empty() && mesh.m_skyVertex.empty()) || mesh.m_index.empty()) {
			ERROR("Actor", "setMesh", "Skipping empty mesh");
			continue;
		}
		if (mesh.m_vertex.size() > static_cast<size_t>(std::numeric_limits<int>::max()) ||
			mesh.m_skyVertex.size() > static_cast<size_t>(std::numeric_limits<int>::max()) ||
			mesh.m_index.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
			ERROR("Actor", "setMesh", "Skipping mesh that exceeds engine mesh counters");
			continue;
		}
		mesh.m_numVertex = static_cast<int>(!mesh.m_vertex.empty() ? mesh.m_vertex.size() : mesh.m_skyVertex.size());
		mesh.m_numIndex = static_cast<int>(mesh.m_index.size());

		Buffer vertexBuffer;
		Buffer indexBuffer;
		if (FAILED(vertexBuffer.init(device, mesh, D3D11_BIND_VERTEX_BUFFER))) {
			ERROR("Actor", "setMesh", "Failed to create vertex buffer");
			continue;
		}
		if (FAILED(indexBuffer.init(device, mesh, D3D11_BIND_INDEX_BUFFER))) {
			ERROR("Actor", "setMesh", "Failed to create index buffer");
			continue;
		}

		m_meshes.push_back(std::move(mesh));
		m_vertexBuffers.push_back(std::move(vertexBuffer));
		m_indexBuffers.push_back(std::move(indexBuffer));
	}
}
