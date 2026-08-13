/**
 * @file MeshComponent.h
 * @brief Declara la API de MeshComponent dentro del subsistema Core.
 * @ingroup core
 */
#pragma once
#include "Prerequisites.h"
#include "ECS/Component.h"
class DeviceContext;
/**
 * @class MeshComponent
 * @brief Componente ECS que almacena la informacin de geometra (malla) de un actor.
 *
 * Un @c MeshComponent contiene los vrtices e ndices que describen la geometra de un objeto.
 * Forma parte del sistema ECS y se asocia a entidades como @c Actor.
 *
 * La malla incluye:
 * - Lista de vrtices (posicin, normal, UV, etc.).
 * - Lista de ndices que definen las primitivas (tringulos, lneas).
 * - Contadores de vrtices e ndices.
 */
class 
MeshComponent : public Component {
public:
  /**
   * @brief Constructor por defecto.
   *
   * Inicializa el componente de malla con cero vrtices e ndices
   * y lo registra como tipo @c MESH en el sistema ECS.
   */
  MeshComponent() : Component(ComponentType::MESH), m_numVertex(0), m_numIndex(0) {
    XMStoreFloat4x4(&m_localTransform, XMMatrixIdentity());
  }

  /**
   * @brief Destructor virtual por defecto.
   */
  virtual 
  ~MeshComponent() = default;

  /**
   * @brief Inicializa el componente de malla.
   *
   * Mtodo heredado de @c Component.
   * Puede usarse para reservar memoria o cargar datos en mallas derivadas.
   */
  void 
  init() override {};

  /**
   * @brief Actualiza la malla.
   *
   * Mtodo heredado de @c Component.
   * til para actualizar animaciones de vrtices, morphing u otros procesos relacionados.
   *
   * @param deltaTime Tiempo transcurrido desde la ltima actualizacin.
   */
  void 
  update(float deltaTime) override {};

  /**
   * @brief Renderiza la malla.
   *
   * Mtodo heredado de @c Component.
   * Normalmente se usara junto con @c DeviceContext para dibujar buffers
   * asociados a la malla.
   *
   * @param deviceContext Contexto del dispositivo para operaciones grficas.
   */
  void 
  render(DeviceContext& deviceContext) override {};

  /**
   * @brief Libera los recursos asociados al componente de malla.
   *
   * Mtodo heredado de @c Component.
   * En implementaciones ms complejas, puede liberar buffers de GPU.
   */
  void
  destroy() override {};

public:
  /**
   * @brief Nombre de la malla.
   */
  std::string m_name;

  /** Material name imported from OBJ/MTL for this submesh. */
  std::string m_materialName;

  /**
   * @brief Lista de vrtices de la malla.
   */
  std::vector<SimpleVertex> m_vertex;
  std::vector<SkyboxVertex> m_skyVertex;

  /**
   * @brief Lista de ndices que definen las primitivas de la malla.
   */
  std::vector<unsigned int> m_index;

  /** Local/model transform imported with the submesh. */
  XMFLOAT4X4 m_localTransform{};

  /**
   * @brief Nmero total de vrtices en la malla.
   */
  int m_numVertex;

  /**
   * @brief Nmero total de ndices en la malla.
   */
  int m_numIndex;
};


