/**
 * @file InputLayout.h
 * @brief Declara la API de InputLayout dentro del subsistema Core.
 * @ingroup core
 */
#pragma once
#include "Prerequisites.h"

class Device;
class DeviceContext;

/**
 * @class InputLayout
 * @brief Encapsula un @c ID3D11InputLayout que describe el formato de los v�rtices para el pipeline de entrada.
 *
 * Un Input Layout en Direct3D 11 define c�mo se interpretan los datos de un Vertex Buffer
 * (posici�n, normales, UVs, colores, etc.) y c�mo se asignan a las entradas de un Vertex Shader.
 *
 * Esta clase administra la creaci�n, uso y destrucci�n del recurso @c ID3D11InputLayout.
 */
class 
InputLayout {
public:
  /**
   * @brief Constructor por defecto.
   */
  InputLayout() = default;

  /**
   * @brief Destructor por defecto.
   * @details No libera autom�ticamente el recurso COM; llamar a destroy().
   */
  ~InputLayout() { destroy(); }

  InputLayout(const InputLayout&) = delete;
  InputLayout& operator=(const InputLayout&) = delete;

  /**
   * @brief Inicializa el Input Layout a partir de una descripci�n y bytecode de Vertex Shader.
   *
   * Crea un @c ID3D11InputLayout utilizando un arreglo de @c D3D11_INPUT_ELEMENT_DESC
   * y el bytecode compilado de un Vertex Shader que define la firma de entrada.
   *
   * @param device            Dispositivo con el que se crea el recurso.
   * @param Layout            Vector con la descripci�n de los elementos de entrada (sem�nticas, formato, offset, etc.).
   * @param VertexShaderData  Bytecode compilado del Vertex Shader que contiene la firma de entrada.
   * @return @c S_OK si la creaci�n fue exitosa; c�digo @c HRESULT en caso de error.
   *
   * @post Si retorna @c S_OK, @c m_inputLayout != nullptr.
   */
  HRESULT 
  init(Device& device,
       const D3D11_INPUT_ELEMENT_DESC* layoutDesc,
       UINT layoutCount,
       ID3DBlob* vertexShaderData);

  /**
   * @brief Actualiza par�metros internos del Input Layout.
   *
   * M�todo de marcador, �til si en el futuro se desea recrear o modificar din�micamente
   * el Input Layout.
   *
   * @note Actualmente no realiza ninguna operaci�n.
   */
  void 
  update();

  /**
   * @brief Aplica el Input Layout al contexto de dispositivo.
   *
   * Asigna el @c ID3D11InputLayout al pipeline gr�fico a trav�s de
   * @c ID3D11DeviceContext::IASetInputLayout.
   *
   * @param deviceContext Contexto donde se establecer� el Input Layout.
   *
   * @pre @c m_inputLayout debe haberse creado con init().
   */
  void 
  render(DeviceContext& deviceContext);

  /**
   * @brief Libera el recurso @c ID3D11InputLayout y deja la instancia en estado no inicializado.
   *
   * Idempotente: puede llamarse m�ltiples veces de forma segura.
   *
   * @post @c m_inputLayout == nullptr.
   */
  void 
  destroy();

public:
  /**
   * @brief Recurso COM de Direct3D 11 que representa el Input Layout.
   * @details V�lido tras init(); @c nullptr despu�s de destroy().
   */
  ID3D11InputLayout* m_inputLayout = nullptr;
};


