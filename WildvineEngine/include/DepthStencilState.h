/**
 * @file DepthStencilState.h
 * @brief Declara la API de DepthStencilState dentro del subsistema Core.
 * @ingroup core
 */
#pragma once
#include "Prerequisites.h"

class Device;
class DeviceContext;

/**
 * @class DepthStencilState
 * @brief Encapsula un @c ID3D11DepthStencilState y su ciclo de vida para la etapa Output-Merger.
 *
 * Administra la creacion, configuracion, aplicacion y destruccion de un estado de
 * profundidad/estencil en Direct3D 11. Permite activar/desactivar pruebas de profundidad y
 * funcionalidades de estencil segun se requiera.
 *
 * @note La clase no posee ni administra la vida de @c Device ni de @c DeviceContext.
 */
class 
DepthStencilState {
public:
  /**
   * @brief Constructor por defecto (no crea recursos).
   */
  DepthStencilState() = default;

  /**
   * @brief Destructor por defecto.
   * @details No libera autom�ticamente el recurso COM; llamar a destroy().
   */
  ~DepthStencilState() { destroy(); }

  DepthStencilState(const DepthStencilState&) = delete;
  DepthStencilState& operator=(const DepthStencilState&) = delete;

  /**
   * @brief Crea el objeto @c ID3D11DepthStencilState a partir de flags comunes.
   *
   * Genera y almacena internamente un estado de profundidad/est�ncil. La configuraci�n
   * concreta (funci�n de comparaci�n, m�scaras, operaciones de est�ncil, etc.) debe definirse
   * en la implementaci�n de este m�todo, condicionada por @p enableDepth y @p enableStencil.
   *
   * @param device         Dispositivo con el que se crea el recurso.
   * @param enableDepth    Habilita la prueba de profundidad (por defecto @c true).
   * @param enableStencil  Habilita el buffer de est�ncil y sus pruebas (por defecto @c false).
   * @return @c S_OK si la creaci�n fue exitosa; c�digo @c HRESULT en caso contrario.
   *
   * @post Si retorna @c S_OK, @c m_depthStencilState != nullptr.
   * @sa render(), destroy()
   */
  HRESULT 
  init(Device& device,
    bool depthEnable,
    D3D11_DEPTH_WRITE_MASK writeMask,
    D3D11_COMPARISON_FUNC depthFunc);

  /**
   * @brief Actualiza par�metros internos si la implementaci�n lo requiere.
   *
   * M�todo placeholder para futuros cambios din�micos del descriptor de profundidad/est�ncil
   * (p. ej., recrear el recurso con otros flags).
   *
   * @note Actualmente no realiza ninguna operaci�n.
   */
  void 
  update();

  /**
   * @brief Aplica el estado de profundidad/est�ncil al contexto (OMSetDepthStencilState).
   *
   * Vincula el @c ID3D11DepthStencilState al @c DeviceContext. Puede adem�s restablecer el estado
   * a @c nullptr si @p reset es @c true.
   *
   * @param deviceContext  Contexto donde se aplicar� el estado.
   * @param stencilRef     Referencia de est�ncil usada por las operaciones de est�ncil (por defecto 0).
   * @param reset          Si es @c true, desvincula el estado despu�s de aplicarlo (setea @c nullptr).
   *
   * @pre @c m_depthStencilState debe haber sido creado con init().
   * @note Internamente invoca @c ID3D11DeviceContext::OMSetDepthStencilState.
   * @sa init(), destroy()
   */
  void 
  render(DeviceContext& deviceContext, unsigned int stencilRef = 0, bool reset = false);

  /**
   * @brief Libera el recurso @c ID3D11DepthStencilState y deja la instancia en estado no inicializado.
   *
   * Idempotente: puede llamarse m�ltiples veces de forma segura.
   *
   * @post @c m_depthStencilState == nullptr.
   */
  void 
  destroy();

private:
  /**
   * @brief Recurso COM de Direct3D 11 para el estado de profundidad/est�ncil.
   * @details V�lido tras @c init(); @c nullptr despu�s de @c destroy().
   */
  ID3D11DepthStencilState* m_depthStencilState = nullptr;
};

