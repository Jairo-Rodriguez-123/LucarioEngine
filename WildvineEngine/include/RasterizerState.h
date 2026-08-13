/**
 * @file RasterizerState.h
 * @brief Declara la API de RasterizerState dentro del subsistema Core.
 * @ingroup core
 */
#pragma once
#include "Prerequisites.h"

class Device;
class DeviceContext;

/**
 * @class Rasterizer
 * @brief Encapsula un @c ID3D11RasterizerState para configurar la etapa de rasterizaci�n en el pipeline gr�fico.
 *
 * La etapa de rasterizaci�n en Direct3D 11 define c�mo se transforman las primitivas
 * (tri�ngulos, l�neas, puntos) en fragmentos antes de pasar al Pixel Shader.
 *
 * Esta clase administra la creaci�n, aplicaci�n y destrucci�n de un @c RasterizerState,
 * permitiendo configurar opciones como el modo de relleno (wireframe/solid), el culling
 * y la profundidad del clipping.
 */
class
RasterizerState {
public:
  /**
   * @brief Constructor por defecto.
   */
  RasterizerState() = default;

  /**
   * @brief Destructor por defecto.
   * @details No libera autom�ticamente el recurso COM; llamar a destroy().
   */
  ~RasterizerState() { destroy(); }

  RasterizerState(const RasterizerState&) = delete;
  RasterizerState& operator=(const RasterizerState&) = delete;

  /**
   * @brief Inicializa el Rasterizer State.
   *
   * Crea un @c ID3D11RasterizerState con una configuraci�n determinada (por ejemplo,
   * @c D3D11_FILL_SOLID y @c D3D11_CULL_BACK).
   * La configuraci�n exacta debe definirse en la implementaci�n del m�todo.
   *
   * @param device Dispositivo con el que se crear� el recurso.
   * @return @c S_OK si la creaci�n fue exitosa; c�digo @c HRESULT en caso de error.
   *
   * @post Si retorna @c S_OK, @c m_rasterizerState != nullptr.
   * @sa render(), destroy()
   */
  HRESULT 
  init(Device& device);
  
  HRESULT 
  init(Device& device,
       D3D11_FILL_MODE fill,
       D3D11_CULL_MODE cull,
       bool frontCCW,
       bool depthClip);

  /**
   * @brief Actualiza par�metros internos del Rasterizer.
   *
   * M�todo de marcador para recrear o modificar din�micamente la configuraci�n
   * del rasterizador.
   *
   * @note Actualmente no realiza ninguna operaci�n.
   */
  void 
  update();

  /**
   * @brief Aplica el Rasterizer State al contexto de dispositivo.
   *
   * Llama a @c ID3D11DeviceContext::RSSetState para establecer el rasterizador activo.
   *
   * @param deviceContext Contexto donde se aplicar� el rasterizer state.
   *
   * @pre @c m_rasterizerState debe haberse creado con init().
   */
  void 
  render(DeviceContext& deviceContext);

  /**
   * @brief Libera el recurso @c ID3D11RasterizerState.
   *
   * Idempotente: puede llamarse m�ltiples veces de forma segura.
   *
   * @post @c m_rasterizerState == nullptr.
   */
  void 
  destroy();

private:
  /**
   * @brief Estado de rasterizaci�n de Direct3D 11.
   * @details V�lido despu�s de init(); @c nullptr tras destroy().
   */
  ID3D11RasterizerState* m_rasterizerState = nullptr;
};


