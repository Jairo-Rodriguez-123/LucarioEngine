/**
 * @file Camera.h
 * @brief Declara la API de Camera dentro del subsistema Utilities.
 * @ingroup utilities
 */
#pragma once
#include "Prerequisites.h"
#include "EngineUtilities/Vectors/Vector3.h"

class 
Camera {
public:
	Camera();
	~Camera() = default;

	/**
		 * @brief Configura la proyecci�n en perspectiva (LH).
		 *
		 * **Pasos**
		 * - Calcula la matriz de proyecci�n con XMMatrixPerspectiveFovLH.
		 * - Guarda FOV, aspect, near y far para debug/inspecci�n.
		 *
		 * **Aplicaci�n pr�ctica**
		 * - Llamar al inicializar ventana y al cambiar resoluci�n.
		 */
	void 
	setLens(float fovYRadians, float aspectRatio, float nearPlane, float farPlane);

	/**
	 * @brief Define posici�n en mundo.
	 */
	void
	setPosition(float x, float y, float z);

	/**
	 * @brief Define posici�n en mundo.
	 */
	void
	setPosition(const EU::Vector3& pos);

	/**
	 * @brief Obtiene la posici�n en mundo.
	 */
	EU::Vector3 
	getPosition() const { return m_position; }

	/**
	 * @brief Fuerza la c�mara a mirar a un objetivo (LH).
	 *
	 * **Pasos**
	 * - Calcula basis a partir de (target - pos).
	 * - Normaliza Forward, Right y Up.
	 * - Marca dirty para recalcular View.
	 *
	 * **Aplicaci�n pr�ctica**
	 * - Cinem�ticas simples o c�maras orbit.
	 */
	void
	lookAt(const EU::Vector3& pos,
				 const EU::Vector3& target, 
				 const EU::Vector3& up = EU::Vector3(0, 1, 0));

	/**
	 * @brief Movimiento relativo a la c�mara (adelante/atr�s).
	 */
	void 
	walk(float d);

	/**
	 * @brief Movimiento relativo a la c�mara (izquierda/derecha).
	 */
	void 
	strafe(float d);

	/**
	 * @brief Rotaci�n sobre el eje Y global (yaw).
	 *
	 * **Aplicaci�n pr�ctica**
	 * - Mouse X para FPS.
	 */
	void 
	yaw(float radians);

	/**
	 * @brief Rotaci�n sobre el eje Right local (pitch).
	 *
	 * **Aplicaci�n pr�ctica**
	 * - Mouse Y para FPS.
	 */
	void 
	pitch(float radians);

	/**
	 * @brief Recalcula la matriz View si es necesario.
	 *
	 * **Pasos**
	 * - Reconstruye basis ortonormal (Right/Up/Forward).
	 * - Calcula View con XMMatrixLookToLH.
	 *
	 * **Aplicaci�n pr�ctica**
	 * - Llamar una vez por frame antes de render.
	 */
	void 
	updateViewMatrix();

	/**
	 * @brief Matriz View (mundo->vista).
	 */
	XMMATRIX 
	getView() const { return XMLoadFloat4x4(&m_view); }

	/**
	 * @brief Matriz Projection (vista->clip).
	 */
	XMMATRIX 
	getProj() const { return XMLoadFloat4x4(&m_proj); }

	/**
	 * @brief View sin traslaci�n (solo rotaci�n). Ideal para Skybox.
	 *
	 * **Aplicaci�n pr�ctica**
	 * - Skybox: ViewNoTranslation * Proj
	 */
	XMMATRIX 
	GetViewNoTranslation() const {
		XMMATRIX v = getView();
		// Quitar traslaci�n (fila 4)
		v.r[3] = XMVectorSet(0, 0, 0, 1);
		return v;
	}

	/**
	 * @brief Devuelve par�metros de proyecci�n (�til para UI/debug).
	 */
	float getFovY()   const { return m_fovY; }
	float getAspect() const { return m_aspectRatio; }
	float getNearZ()  const { return m_nearPlane; }
	float getFarZ()   const { return m_farPlane; }

	/**
	 * @brief Vectores base (mundo) de la c�mara.
	 */
	EU::Vector3 GetRight()   const { return m_right; }
	EU::Vector3 GetUp()      const { return m_up; }
	EU::Vector3 GetForward() const { return m_forward; }

	inline EU::Vector3 
	FromXM(FXMVECTOR v)	{
		XMFLOAT3 t;
		XMStoreFloat3(&t, v);
		return EU::Vector3(t.x, t.y, t.z);
	}


private:
	// Estado espacial
	EU::Vector3 m_position; /**< The position of the camera in world space. */

	// Basis Ortonormal (en mundo)
	EU::Vector3 m_right		{ 1.0f, 0.0f, 0.0f }; /**< The right vector of the camera's orthonormal basis. */
	EU::Vector3 m_up			{ 0.0f, 1.0f, 0.0f }; /**< The up vector of the camera's orthonormal basis. */
	EU::Vector3 m_forward	{ 0.0f, 0.0f, 1.0f }; /**< The forward vector of the camera's orthonormal basis. */

	// Matrices almacenadas
	XMFLOAT4X4 m_view {};
	XMFLOAT4X4 m_proj {};

	// Parametros de la proyeccion
	float m_fovY { XM_PIDIV4 }; /**< The field of view (FOV) angle in radians. */
	float m_aspectRatio = 1.0f; /**< The aspect ratio of the camera's view (width divided by height). */
	float m_nearPlane = 0.01f; /**< The distance to the near clipping plane. */
	float m_farPlane = 1000.0f; /**< The distance to the far clipping plane. */

	bool m_viewDirty = true; /**< Flag indicating whether the view matrix needs to be recalculated. */
};


