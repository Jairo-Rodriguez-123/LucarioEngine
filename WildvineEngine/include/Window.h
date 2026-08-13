/**
 * @file Window.h
 * @brief Declara la API de Window dentro del subsistema Core.
 * @ingroup core
 */
#pragma once
#include "Prerequisites.h"


class BaseApp;

/**
 * @class Window
 * @brief Encapsula la creacion y administracion de la ventana principal de Win32.
 *
 * Esta clase concentra la creacion del `HWND`, el tamano util del cliente y los
 * datos necesarios para inicializar el pipeline grafico del motor.
 */
class 
Window {
public:
	Window()  = default;
	~Window() { destroy(); }
	Window(const Window&) = delete;
	Window& operator=(const Window&) = delete;

	/**
	 * @brief Crea y muestra la ventana principal del motor.
	 * @param hInstance Instancia actual de la aplicacion Win32.
	 * @param nCmdShow Modo de visualizacion inicial solicitado por Windows.
	 * @param wndproc Procedimiento de ventana asociado al `HWND`.
	 * @param app Instancia propietaria usada como `lpCreateParams`.
	 * @return `S_OK` si la ventana se crea correctamente; `HRESULT` de error en caso contrario.
	 */
	HRESULT 
	init(HINSTANCE hInstance, int nCmdShow, WNDPROC wndproc, BaseApp* app);

	void 
	update();
	
	void 
	render();
	
	void 
	destroy();

public:
	HWND m_hWnd = nullptr;      ///< Handle de la ventana nativa.
	unsigned int m_width = 0;       ///< Ancho actual del area cliente.
	unsigned int m_height = 0;      ///< Alto actual del area cliente.
private:
	HINSTANCE m_hInst = nullptr;
	RECT m_rect;
	std::string m_windowName = "Wildvine Engine";
};


