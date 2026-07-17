/**
 * @file BaseApp.h
 * @brief Declara la API de BaseApp dentro del subsistema Core.
 * @ingroup core
 */
#pragma once
#include "Prerequisites.h"
#include "Window.h"
#include "Device.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "Texture.h"
#include "RenderTargetView.h"
#include "DepthStencilView.h"
#include "Viewport.h"
#include "ShaderProgram.h"
#include "MeshComponent.h"
#include "Buffer.h"
#include "SamplerState.h"
#include "Model3D.h"
#include "ECS/Actor.h"
#include "EngineUtilities\GUI/GUI.h"
#include "SceneGraph\SceneGraph.h"
#include "EngineUtilities\Utilities\Camera.h"
#include "EngineUtilities\Utilities\Skybox.h"
#include "EngineUtilities\Utilities\LayoutBuilder.h"
#include "EngineUtilities/Utilities/EditorViewportPass.h"
#include "ECS/LightComponent.h"
#include "ECS/MeshRendererComponent.h"
#include "Rendering/Material.h"
#include "Rendering/MaterialInstance.h"
#include "Rendering/Mesh.h"
#include "Rendering/RenderPipeline.h"
#include "Rendering/RenderScene.h"
#include <string>
extern IMGUI_IMPL_API
LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

/**
 * @class BaseApp
 * @brief Coordina el ciclo de vida principal de Wildvine Engine.
 *
 * `BaseApp` inicializa la ventana, el dispositivo grafico, la interfaz del editor,
 * la escena de prueba y el pipeline de render. Tambien administra el bucle principal
 * de actualizacion, la serializacion basica de escena y la respuesta a cambios de tamano.
 */
class
	BaseApp {
public:
	BaseApp() = default;
	~BaseApp() { destroy(); }

	/**
	 * @brief Prepara subsistemas previos al render.
	 * @return `S_OK` si la aplicacion queda lista para continuar la inicializacion.
	 */
	HRESULT
		awake();

	/**
	 * @brief Ejecuta el bucle principal de la aplicacion.
	 * @param hInst Instancia Win32 actual.
	 * @param nCmdShow Modo inicial de visualizacion de la ventana.
	 * @return Codigo de salida del proceso.
	 */
	int
		run(HINSTANCE hInst, int nCmdShow);

	/**
	 * @brief Inicializa recursos graficos, escena, materiales y renderer.
	 * @return `S_OK` cuando todos los recursos base quedan listos.
	 */
	HRESULT
		init();

	/**
	 * @brief Ejecuta la logica por frame y sincroniza GUI, camara y escena.
	 * @param deltaTime Tiempo transcurrido desde el frame anterior.
	 */
	void
		update(float deltaTime);

	/**
	 * @brief Emite el frame actual en el viewport del editor y en el back buffer final.
	 */
	void
		render();

	/**
	 * @brief Libera recursos del motor en orden seguro de destruccion.
	 */
	void
		destroy();

	/**
	 * @brief Reconstuye recursos dependientes de la resolucion principal.
	 * @param newW Nuevo ancho del area cliente.
	 * @param newH Nuevo alto del area cliente.
	 */
	void
		onResize(unsigned int newW, unsigned int newH);

	/**
	 * @brief Atiende cambios diferidos del viewport interno del editor.
	 */
	void handleEditorViewportResize();

	/**
	 * @brief Serializa la escena actual a disco.
	 * @param path Ruta de salida del archivo `.wvscene`.
	 * @return `true` si la escena se guarda correctamente.
	 */
	bool saveScene(const std::string& path);

	/**
	 * @brief Carga una escena serializada previamente.
	 * @param path Ruta del archivo `.wvscene`.
	 * @return `true` si el contenido se pudo leer y aplicar.
	 */
	bool loadScene(const std::string& path);

	/**
	 * @brief Devuelve la ruta por defecto usada por el editor para persistencia rapida.
	 */
	std::string getDefaultScenePath() const;
private:
	EU::TSharedPointer<Actor> createLightActor(const std::string& name = std::string());

	static LRESULT CALLBACK
		WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);


private:
	Window                              m_window;
	Device															m_device;
	DeviceContext										m_deviceContext;
	SwapChain                           m_swapChain;
	Texture                             m_backBuffer;
	RenderTargetView									  m_renderTargetView;
	Texture                             m_depthStencil;
	DepthStencilView									  m_depthStencilView;
	Viewport                            m_viewport;
	ShaderProgram												m_shaderProgram;
	//Buffer															m_cbNeverChanges;
	//Buffer															m_cbChangeOnResize;
	bool m_d3dReady = false;
	Buffer m_constantBuffer;
	CBMain m_constantBufferStruct;

	// Textures
	Texture m_AlbedoSRV;
	Texture m_MetallicSRV;
	Texture m_RoughnessSRV;
	Texture m_AOSRV;
	Texture m_NormalSRV;
	Texture m_EmissiveSRV;
	Texture m_drakefireAlbedoSRV;
	Texture m_drakefireNormalSRV;
	Texture m_drakefireMetallicSRV;
	Texture m_drakefireRoughnessSRV;
	Texture m_drakefireAOSRV;
	Texture m_toadAlbedoSRV;
	Texture m_toadNormalSRV;
	Texture m_toadMetallicSRV;
	Texture m_toadRoughnessSRV;
	Texture m_toadAOSRV;
	Texture m_toadGlassAlbedoSRV;
	Texture m_toadGlassNormalSRV;
	Texture m_toadGlassRoughnessSRV;
	Texture m_toadHeadAlbedoSRV;
	Texture m_toadHeadNormalSRV;
	Texture m_toadHeadRoughnessSRV;

	Camera															m_camera;

	SceneGraph												m_sceneGraph;
	std::vector<EU::TSharedPointer<Actor>> m_actors;
	EU::TSharedPointer<Actor> m_drakefirePistol;
	EU::TSharedPointer<Actor> m_sciFiToad;
	EU::TSharedPointer<Actor> m_directionalLightActor;


	Model3D* m_model = nullptr;
	Model3D* m_drakefireModel = nullptr;
	Model3D* m_toadModel = nullptr;

	//CBChangeOnResize										cbChangesOnResize;
	//CBNeverChanges											cbNeverChanges;
	GUI																m_gui;
	bool m_guiInitialized = false;
	EU::Vector3 m_cameraPos;

	Skybox m_skybox;
	Texture															m_skyboxTex;
	Texture m_lightIconTexture;
	RasterizerState m_defaultRasterizer;
	DepthStencilState m_defaultDepthStencil;
	SamplerState m_defaultSampler;
	Mesh m_cyberGunRenderMesh;
	Mesh m_drakefireRenderMesh;
	Mesh m_toadRenderMesh;
	Material m_pbrMaterial;
	Material m_transparentPbrMaterial;
	Material m_cyberGunPbrMaterial;
	Material m_drakefirePbrMaterial;
	Material m_toadPbrMaterial;
	Material m_toadGlassPbrMaterial;
	Material m_toadHeadPbrMaterial;
	MaterialInstance m_cyberGunMaterial;
	MaterialInstance m_drakefireMaterial;
	MaterialInstance m_toadMaterial;
	MaterialInstance m_toadGlassMaterial;
	MaterialInstance m_toadHeadMaterial;

	EditorViewportPass m_editorViewportPass;
	RenderPipeline m_renderPipeline;
	RenderScene m_renderScene;
	bool m_editorViewportResizePending = false;
	unsigned int m_pendingViewportWidth = 1;
	unsigned int m_pendingViewportHeight = 1;

	unsigned int m_lastRequestedViewportWidth = 1;
	unsigned int m_lastRequestedViewportHeight = 1;
	int m_viewportResizeStableFrames = 0;
};


