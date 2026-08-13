/**
 * @file SamplerState.h
 * @brief Declara la API de SamplerState dentro del subsistema Core.
 * @ingroup core
 */
#pragma once
#include "Prerequisites.h"

class Device;
class DeviceContext;

/**
 * @class SamplerState
 * @brief Encapsula un @c ID3D11SamplerState para la etapa de muestreo de texturas en Direct3D 11.
 *
 * Un Sampler State define c�mo se leen las texturas en los shaders:
 * - Filtrado (point, linear, anisotropic).
 * - Direccionamiento (wrap, mirror, clamp, border).
 * - Nivel de mipmapping.
 *
 * Esta clase administra la creaci�n, aplicaci�n y destrucci�n de un @c SamplerState.
 */
class 
SamplerState {
public:
  /**
   * @brief Constructor por defecto.
   */
  SamplerState() = default;

  /**
   * @brief Destructor por defecto.
   * @details No libera autom�ticamente el recurso COM; llamar a destroy().
   */
  ~SamplerState() { destroy(); }

  SamplerState(const SamplerState&) = delete;
  SamplerState& operator=(const SamplerState&) = delete;

  /**
   * @brief Inicializa el Sampler State con una configuraci�n predeterminada.
   *
   * Crea un @c ID3D11SamplerState configurado seg�n la implementaci�n (ejemplo:
   * filtrado lineal, wrap en UV, LOD completo).
   *
   * @param device Dispositivo con el que se crear� el recurso.
   * @return @c S_OK si fue exitoso; c�digo @c HRESULT en caso de error.
   *
   * @post Si retorna @c S_OK, @c m_sampler != nullptr.
   * @sa render(), destroy()
   */
  HRESULT 
  init(Device& device);

  /**
   * @brief Actualiza par�metros internos del Sampler.
   *
   * M�todo de marcador para recrear o mutar din�micamente la configuraci�n
   * (por ejemplo, cambiar de filtrado linear a anisotr�pico).
   *
   * @note Actualmente no realiza ninguna operaci�n.
   */
  void 
  update();

  /**
   * @brief Asigna el Sampler State a la etapa de Pixel Shader.
   *
   * Llama a @c ID3D11DeviceContext::PSSetSamplers para establecer el sampler.
   *
   * @param deviceContext Contexto donde se aplicar� el sampler.
   * @param StartSlot     Slot inicial en el que se vincular� el sampler.
   * @param NumSamplers   N�mero de samplers a enlazar (normalmente 1).
   *
   * @pre @c m_sampler debe haberse creado con init().
   */
  void 
  render(DeviceContext& deviceContext,
         unsigned int StartSlot,
         unsigned int NumSamplers);

  /**
   * @brief Libera el recurso @c ID3D11SamplerState.
   *
   * Idempotente: puede llamarse m�ltiples veces de forma segura.
   *
   * @post @c m_sampler == nullptr.
   */
  void 
  destroy();

public:
  /**
   * @brief Recurso COM de Direct3D 11 para el Sampler State.
   * @details V�lido tras init(); @c nullptr despu�s de destroy().
   */
  ID3D11SamplerState* m_sampler = nullptr;
};


