/**
 * @file Actor.h
 * @brief Declara la API de Actor dentro del subsistema ECS.
 * @ingroup ecs
 */
#pragma once
#include "Prerequisites.h"
#include "Entity.h"
#include "Buffer.h"
#include "Texture.h"
#include "Transform.h"
#include "SamplerState.h"
#include "RasterizerState.h"
//#include "BlendState.h"
#include "ShaderProgram.h"
#include "DepthStencilState.h"

class Device;
class DeviceContext;
class MeshComponent;

/**
 * @class Actor
 * @brief Representa una entidad gr�fica con mallas, texturas y estados de renderizado.
 *
 * Un Actor es una entidad del motor que contiene mallas, texturas y recursos de renderizado
 * necesarios para dibujar un objeto en la escena.
 * Administra buffers de v�rtices e �ndices, estados de rasterizaci�n, blending y shaders,
 * adem�s de soportar renderizado de sombras.
 */
class 
Actor : public Entity {
public:
  /**
   * @brief Constructor por defecto.
   */
  Actor();

  /**
   * @brief Constructor que inicializa el actor con un dispositivo.
   * @param device Dispositivo usado para inicializar recursos del actor.
   */
  Actor(Device& device);

  /**
   * @brief Destructor virtual por defecto.
   */
  virtual 
  ~Actor() override { destroy(); }

  void
  awake() override {}

  /**
   * @brief Inicializa el actor.
   *
   * M�todo heredado de @c Entity.
   * Puede usarse para inicializar recursos adicionales en clases derivadas.
   */
  void 
  init() override {}

  /**
   * @brief Actualiza el actor en cada frame.
   *
   * @param deltaTime     Tiempo transcurrido desde la �ltima actualizaci�n.
   * @param deviceContext Contexto del dispositivo para operaciones gr�ficas.
   *
   * @note Este m�todo puede actualizar animaciones, transformaciones u otros recursos dependientes del tiempo.
   */
  void 
  update(float deltaTime, DeviceContext& deviceContext) override;

  /**
   * @brief Renderiza el actor en la escena.
   *
   * Configura estados de render, buffers y shaders antes de dibujar las mallas asociadas al actor.
   *
   * @param deviceContext Contexto del dispositivo para operaciones gr�ficas.
   */
  void 
  render(DeviceContext& deviceContext) override;

  void 
  renderForSkybox(DeviceContext& deviceContext);

  /**
   * @brief Libera todos los recursos asociados al actor.
   *
   * Incluye buffers, estados, shaders y texturas.
   */
  void 
  destroy();

  /**
   * @brief Establece las mallas del actor.
   *
   * Inicializa buffers de v�rtices e �ndices asociados a las mallas.
   *
   * @param device Dispositivo con el cual se inicializan las mallas.
   * @param meshes Vector de componentes de malla que se asignar�n al actor.
   */
  void 
  setMesh(Device& device, std::vector<MeshComponent> meshes);

  /**
   * @brief Obtiene el nombre del actor.
   * @return Nombre actual del actor.
   */
  std::string 
  getName() { return m_name; }

  /**
   * @brief Establece el nombre del actor.
   * @param name Nuevo nombre para el actor.
   */
  void 
  setName(const std::string& name) { m_name = name; }

  /**
   * @brief Establece las texturas del actor.
   * @param textures Vector de texturas a asignar al actor.
   */
  void 
  setTextures(const std::vector<Texture>& textures) { m_textures = textures; }

  /**
   * @brief Define si el actor proyecta sombras.
   * @param v Valor booleano que habilita o deshabilita las sombras.
   */
  void 
  setCastShadow(bool v) { castShadow = v; }

  /**
   * @brief Indica si el actor puede proyectar sombras.
   * @return @c true si el actor proyecta sombras; @c false en caso contrario.
   */
  bool 
  canCastShadow() const { return castShadow; }

  /**
   * @brief Renderiza la sombra del actor.
   *
   * Usa shaders y estados espec�ficos de shadow mapping para dibujar la proyecci�n del actor en el mapa de sombras.
   *
   * @param deviceContext Contexto del dispositivo para operaciones gr�ficas.
   */
  void 
  renderShadow(DeviceContext& deviceContext);

private:
  void syncModelBuffer(DeviceContext& deviceContext);

private:
  std::vector<MeshComponent> m_meshes;   ///< Conjunto de componentes de malla del actor.
  std::vector<Texture> m_textures;       ///< Texturas aplicadas al actor.
  size_t m_lastTextureSlotCount = 0;     ///< Slots usados en el draw anterior para poder desasignar SRVs obsoletos.
  std::vector<Buffer> m_vertexBuffers;   ///< Buffers de v�rtices asociados a las mallas.
  std::vector<Buffer> m_indexBuffers;    ///< Buffers de �ndices asociados a las mallas.

  //BlendState m_blendstate;               ///< Estado de blending usado por el actor.
  //RasterizerState m_rasterizer;               ///< Estado de rasterizaci�n usado por el actor.
  SamplerState m_sampler;                ///< Estado de muestreo de texturas.
  CBChangesEveryFrame m_model;           ///< Constante de buffer para transformaciones por frame.
  Buffer m_modelBuffer;                  ///< Constant buffer que contiene @c m_model.

  // Recursos para sombras
  ShaderProgram m_shaderShadow;          ///< Shader program usado para renderizar sombras.
  Buffer m_shaderBuffer;                 ///< Buffer auxiliar para datos de sombras.
  //BlendState m_shadowBlendState;         ///< Estado de blending espec�fico para sombras.
  DepthStencilState m_shadowDepthStencilState; ///< Estado de profundidad/est�ncil para sombras.
  CBChangesEveryFrame m_cbShadow;        ///< Constant buffer espec�fico de sombras.

  XMFLOAT4 m_LightPos;                   ///< Posici�n de la luz usada para proyectar sombras.
  std::string m_name = "Actor";          ///< Nombre identificador del actor.
  bool castShadow = true;                ///< Indica si el actor proyecta sombras.
};


