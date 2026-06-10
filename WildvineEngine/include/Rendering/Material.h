#pragma once
#include "Prerequisites.h"
#include "Rendering/RenderTypes.h"

class ShaderProgram;
class RasterizerState;
class DepthStencilState;
class SamplerState;

class
Material {
public:
	/*
		*  @brief Set the shader program used by this material.
	*/
	void 
	setShader(ShaderProgram* shader) { m_shader = shader; }
	/*
		*  @brief Set the rasterizer state for this material.
	*/
	void setRasterizerState(RasterizerState* state) { m_rasterizerState = state; }
	/*
		*  @brief Set the depth-stencil state for this material.
	*/
	void setDepthStencilState(DepthStencilState* state) { m_depthStencilState = state; }
	/*
		*  @brief Set the sampler state used by this material.
	*/
	void setSamplerState(SamplerState* state) { m_samplerState = state; }
	/*
		*  @brief Set the material domain (e.g., opaque, translucent).
	*/
	void setDomain(MaterialDomain domain) { m_domain = domain; }
	/*
		*  @brief Set the blend mode for rendering this material.
	*/
	void setBlendMode(BlendMode blendMode) { m_blendMode = blendMode; }

	/*
		*  @brief Get the shader program used by this material.
	*/
	ShaderProgram* getShader() const { return m_shader; }
	/*
		*  @brief Get the rasterizer state associated with this material.
	*/
	RasterizerState* getRasterizerState() const { return m_rasterizerState; }
	/*
		*  @brief Get the depth-stencil state associated with this material.
	*/
	DepthStencilState* getDepthStencilState() const { return m_depthStencilState; }
	/*
		*  @brief Get the sampler state associated with this material.
	*/
	SamplerState* getSamplerState() const { return m_samplerState; }
	/*
		*  @brief Get the material domain.
	*/
	MaterialDomain getDomain() const { return m_domain; }
	/*
		*  @brief Get the blend mode.
	*/
	BlendMode getBlendMode() const { return m_blendMode; }

private:
	/*
		*  @brief Pointer to the shader program used by this material. May be null.
	*/
	ShaderProgram* m_shader = nullptr;                   
	/*
		*  @brief Pointer to the rasterizer state. May be null.
	*/
	RasterizerState* m_rasterizerState = nullptr;        
	/*
		*  @brief Pointer to the depth-stencil state. May be null.
	*/
	DepthStencilState* m_depthStencilState = nullptr;    
	/*
		*  @brief Pointer to the sampler state. May be null.
	*/
	SamplerState* m_samplerState = nullptr;              
	/*
		*  @brief Material domain (defaults to Opaque).
	*/
	MaterialDomain m_domain = MaterialDomain::Opaque;    
	/*
		*  @brief Blend mode for the material (defaults to Opaque).
	*/
	BlendMode m_blendMode = BlendMode::Opaque;           
};


