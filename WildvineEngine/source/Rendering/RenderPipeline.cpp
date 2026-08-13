#include "Rendering/RenderPipeline.h"

HRESULT
RenderPipeline::init(Device& device, RendererType initialRenderer) {
	destroy();
	m_activeRenderer = nullptr;
	m_activeRendererType = initialRenderer;
	m_forwardInitialized = false;
	m_deferredInitialized = false;
	m_lastWidth = 1280;
	m_lastHeight = 720;
	return setRendererType(initialRenderer, device);
}

HRESULT
RenderPipeline::setRendererType(RendererType rendererType, Device& device) {
	HRESULT hr = ensureRendererInitialized(rendererType, device);
	if (FAILED(hr)) {
		return hr;
	}

	ISceneRenderer* candidateRenderer = resolveRenderer(rendererType);
	if (!candidateRenderer) {
		return E_FAIL;
	}

	hr = candidateRenderer->resize(device, m_lastWidth, m_lastHeight);
	if (FAILED(hr)) {
		return hr;
	}

	m_activeRendererType = rendererType;
	m_activeRenderer = candidateRenderer;
	MESSAGE("RenderPipeline", "setRendererType", m_activeRenderer->getDebugName());
	return S_OK;
}

HRESULT
RenderPipeline::resize(Device& device, unsigned int width, unsigned int height) {
	if (width == 0 || height == 0) {
		return E_INVALIDARG;
	}
	if (!m_activeRenderer) {
		return E_UNEXPECTED;
	}

	const HRESULT hr = m_activeRenderer->resize(device, width, height);
	if (FAILED(hr)) {
		return hr;
	}

	m_lastWidth = width;
	m_lastHeight = height;
	return S_OK;
}

void
RenderPipeline::render(DeviceContext& deviceContext,
	const Camera& camera,
	RenderScene& scene,
	EditorViewportPass& viewportPass) {
	if (m_activeRenderer) {
		m_activeRenderer->render(deviceContext, camera, scene, viewportPass);
	}
}

void
RenderPipeline::destroy() {
	if (m_deferredInitialized) {
		m_deferredRenderer.destroy();
		m_deferredInitialized = false;
	}

	if (m_forwardInitialized) {
		m_forwardRenderer.destroy();
		m_forwardInitialized = false;
	}

	m_activeRenderer = nullptr;
	m_lastWidth = 1280;
	m_lastHeight = 720;
}

const char*
RenderPipeline::getActiveRendererName() const {
	const ISceneRenderer* renderer = m_activeRenderer;
	return renderer ? renderer->getDebugName() : "NoRenderer";
}

ID3D11ShaderResourceView*
RenderPipeline::getShadowMapSRV() const {
	const ISceneRenderer* renderer = m_activeRenderer;
	return renderer ? renderer->getShadowMapSRV() : nullptr;
}

ID3D11ShaderResourceView*
RenderPipeline::getPreShadowSRV() const {
	const ISceneRenderer* renderer = m_activeRenderer;
	return renderer ? renderer->getPreShadowSRV() : nullptr;
}

ID3D11ShaderResourceView*
RenderPipeline::getGBufferAlbedoMetallicSRV() const {
	const ISceneRenderer* renderer = m_activeRenderer;
	return renderer ? renderer->getGBufferAlbedoMetallicSRV() : nullptr;
}

ID3D11ShaderResourceView*
RenderPipeline::getGBufferNormalRoughnessSRV() const {
	const ISceneRenderer* renderer = m_activeRenderer;
	return renderer ? renderer->getGBufferNormalRoughnessSRV() : nullptr;
}

ID3D11ShaderResourceView*
RenderPipeline::getGBufferWorldAoSRV() const {
	const ISceneRenderer* renderer = m_activeRenderer;
	return renderer ? renderer->getGBufferWorldAoSRV() : nullptr;
}

ID3D11ShaderResourceView*
RenderPipeline::getGBufferEmissiveAlphaSRV() const {
	const ISceneRenderer* renderer = m_activeRenderer;
	return renderer ? renderer->getGBufferEmissiveAlphaSRV() : nullptr;
}

void
RenderPipeline::setShadowFactorDebugEnabled(bool enabled) {
	ISceneRenderer* renderer = resolveRenderer(RendererType::Deferred);
	if (renderer) {
		renderer->setShadowFactorDebugEnabled(enabled);
	}
}

void
RenderPipeline::setDeferredDebugViewMode(int mode) {
	ISceneRenderer* renderer = resolveRenderer(RendererType::Deferred);
	if (renderer) {
		renderer->setDeferredDebugViewMode(mode);
	}
}

HRESULT
RenderPipeline::ensureRendererInitialized(RendererType rendererType, Device& device) {
	switch (rendererType) {
	case RendererType::Forward:
		if (!m_forwardInitialized) {
			HRESULT hr = m_forwardRenderer.init(device);
			if (FAILED(hr)) {
				m_forwardRenderer.destroy();
				return hr;
			}
			m_forwardInitialized = true;
		}
		return S_OK;
	case RendererType::Deferred:
		if (!m_deferredInitialized) {
			HRESULT hr = m_deferredRenderer.init(device);
			if (FAILED(hr)) {
				m_deferredRenderer.destroy();
				return hr;
			}
			m_deferredInitialized = true;
		}
		return S_OK;
	default:
		return E_INVALIDARG;
	}
}

ISceneRenderer*
RenderPipeline::resolveRenderer(RendererType rendererType) {
	switch (rendererType) {
	case RendererType::Forward:
		return &m_forwardRenderer;
	case RendererType::Deferred:
		return &m_deferredRenderer;
	default:
		return nullptr;
	}
}

const ISceneRenderer*
RenderPipeline::resolveRenderer(RendererType rendererType) const {
	switch (rendererType) {
	case RendererType::Forward:
		return &m_forwardRenderer;
	case RendererType::Deferred:
		return &m_deferredRenderer;
	default:
		return nullptr;
	}
}