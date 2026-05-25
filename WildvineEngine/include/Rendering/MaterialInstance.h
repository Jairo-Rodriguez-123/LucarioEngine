/**
 * @file MaterialInstance.h
 * @brief Declara la API de MaterialInstance dentro del subsistema Rendering.
 * @ingroup rendering
 */
#pragma once
#include "Prerequisites.h"
#include "Rendering/RenderTypes.h"

class Material;
class DeviceContext;
class Texture;

/**
 * @class MaterialInstance
 * @brief Agrupa un material base con sus texturas y parametros concretos.
 *
 * Esta clase permite reutilizar un mismo `Material` con diferentes mapas de texturas
 * y parametros PBR por objeto renderizado.
 */
class
MaterialInstance {
public:
	void setMaterial(Material* material) { m_material = material; }
  void setAlbeto(Texture* texture) { m_albedo = texture; }
  void setNormal(Texture* texture) { m_normal = texture; }
  void setMetallic(Texture* texture) { m_metallic = texture; }
  void setRoughness(Texture* texture) { m_roughness = texture; }
  void setAO(Texture* texture) { m_ao = texture; }
  void setEmissive(Texture* texture) { m_emissive = texture; }

  Material* getMaterial() const { return m_material; }
  Texture* getAlbedo() const { return m_albedo; }
  Texture* getNormal() const { return m_normal; }
  Texture* getMetallic() const { return m_metallic; }
  Texture* getRoughness() const { return m_roughness; }
  Texture* getAO() const { return m_ao; }
  Texture* getEmissive() const { return m_emissive; }

  MaterialParams& getParams() { return m_params; }
  const MaterialParams& getParams() const { return m_params; }

  void bindTextures(DeviceContext&  deviceContext) const;

private:
  Material* m_material = nullptr; ///< Material base compartido por esta instancia.
  Texture* m_albedo = nullptr;    ///< Textura de albedo (color base).
  Texture* m_normal = nullptr;    ///< Textura de normales.
  Texture* m_metallic = nullptr;  ///< Textura de metalicidad.
  Texture* m_roughness = nullptr; ///< Textura de rugosidad.
  Texture* m_ao = nullptr;        ///< Textura de oclusión ambiental.
  Texture* m_emissive = nullptr;  ///< Textura de emisividad.
  MaterialParams m_params;         ///< Parámetros PBR específicos de esta instancia.
};



