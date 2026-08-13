#pragma once
#include "Prerequisites.h"
#include "Rendering/RenderTypes.h"

class Material;
class DeviceContext;
class Texture;

class
MaterialInstance {
public:
	/*
	 *  @brief Sets the material used by this instance.
	*/
	void setMaterial(Material* material) { m_material = material; }
  /*
	 *  @brief Assigns the albedo (base color) texture for this material instance.
	*/
  void setAlbedo(Texture* texture) { m_albedo = texture; }
  // Compatibility with older engine code that used the misspelled API.
  void setAlbeto(Texture* texture) { setAlbedo(texture); }
  /*
	 *  @brief Assigns the normal map texture for this material instance.
	*/
  void setNormal(Texture* texture) { m_normal = texture; }
  /*
	 *  @brief Assigns the metallic texture (metalness map) for this material instance.
	*/
  void setMetallic(Texture* texture) { m_metallic = texture; }
  /*
	 *  @brief Assigns the roughness texture for this material instance.
	*/
  void setRoughness(Texture* texture) { m_roughness = texture; }
  /*
	 *  @brief Assigns the ambient occlusion (AO) texture for this material instance.
	*/
  void setAO(Texture* texture) { m_ao = texture; }
  /*
	 *  @brief Assigns the emissive texture for this material instance.
	*/
  void setEmissive(Texture* texture) { m_emissive = texture; }

  /*
	 *  @brief Returns the material assigned to this instance.
	*/
  Material* getMaterial() const { return m_material; }
  /*
	 *  @brief Returns the albedo (base color) texture assigned to this instance.
	*/
  Texture* getAlbedo() const { return m_albedo; }
  /*
	 *  @brief Returns the normal map texture assigned to this instance.
	*/
  Texture* getNormal() const { return m_normal; }
  /*
	 *  @brief Returns the metallic texture (metalness map) assigned to this instance.
	*/
  Texture* getMetallic() const { return m_metallic; }
  /*
	 *  @brief Returns the roughness texture assigned to this instance.
	*/
  Texture* getRoughness() const { return m_roughness; }
  /*
	 *  @brief Returns the ambient occlusion (AO) texture assigned to this instance.
	*/
  Texture* getAO() const { return m_ao; }
  /*
	 *  @brief Returns the emissive texture assigned to this instance.
	*/
  Texture* getEmissive() const { return m_emissive; }

  /*
	 *  @brief Returns a mutable reference to the material parameters.
	*/
  MaterialParams& getParams() { return m_params; }
  /*
	 *  @brief Returns a const reference to the material parameters.
	*/
  const MaterialParams& getParams() const { return m_params; }

  /*
	 *  @brief Bind the textures of this material instance to the given device context for rendering.
	 *
	 *  @param deviceContext Reference to the device context where textures will be bound.
	*/
  void bindTextures(DeviceContext&  deviceContext) const;

private:
  /*
	 *  @brief Pointer to the material resource used by this instance.
	*/
  Material* m_material = nullptr; 
  /*
	 *  @brief Pointer to the albedo (base color) texture.
	*/
  Texture* m_albedo = nullptr;    
  /*
	 *  @brief Pointer to the normal map texture.
	*/
  Texture* m_normal = nullptr;    
  /*
	 *  @brief Pointer to the metallic map texture.
	*/
  Texture* m_metallic = nullptr;  
  /*
	 *  @brief Pointer to the roughness map texture.
	*/
  Texture* m_roughness = nullptr; 
  /*
	 *  @brief Pointer to the ambient occlusion (AO) texture.
	*/
  Texture* m_ao = nullptr;        
  /*
	 *  @brief Pointer to the emissive texture.
	*/
  Texture* m_emissive = nullptr;  
  /*
	 *  @brief Material-specific scalar/vector parameters used by shaders.
	*/
  MaterialParams m_params;         
};



