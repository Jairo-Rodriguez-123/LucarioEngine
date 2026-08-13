/**
 * @file EditorViewportPass.h
 * @brief Declara la API de EditorViewportPass dentro del subsistema Utilities.
 * @ingroup utilities
 */
#pragma once
#include "Prerequisites.h"
#include "Texture.h"
#include "RenderTargetView.h"
#include "DepthStencilView.h"

class Device;
class DeviceContext;

class 
EditorViewportPass {
public:
	EditorViewportPass() = default;
	~EditorViewportPass() { destroy(); }

	HRESULT init(Device& device, unsigned int width, unsigned int height);
	HRESULT resize(Device& device, unsigned int width, unsigned int height);

	void begin(DeviceContext& deviceContext, const float clearColor[4]);
	void swap(EditorViewportPass& other);
	void clearDepth(DeviceContext& deviceContext);
	void setViewport(DeviceContext& deviceContext);
	void destroy();

	ID3D11ShaderResourceView* getSRV() const { return m_colorSRV.m_textureFromImg; }
	ID3D11RenderTargetView* getRTV() const { return m_rtv.get(); }
	ID3D11DepthStencilView* getDSV() const { return m_dsv.get(); }

	unsigned int getWidth() const { return m_width; }
	unsigned int getHeight() const { return m_height; }

	bool isValid() const
	{
		return m_colorTexture.m_texture != nullptr &&
			m_colorSRV.m_textureFromImg != nullptr &&
			m_rtv.get() != nullptr &&
			m_depthTexture.m_texture != nullptr &&
			m_dsv.get() != nullptr;
	}

private:
	HRESULT createResources(Device& device, unsigned int width, unsigned int height);

private:
	Texture           m_colorTexture;
	Texture           m_colorSRV;
	RenderTargetView  m_rtv;

	Texture           m_depthTexture;
	DepthStencilView  m_dsv;

	unsigned int      m_width = 1;
	unsigned int      m_height = 1;
};

