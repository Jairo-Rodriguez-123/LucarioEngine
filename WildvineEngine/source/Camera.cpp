/**
 * @file Camera.cpp
 * @brief Implementa la logica de Camera dentro del subsistema Utilities.
 * @ingroup utilities
 */
#include "EngineUtilities/Utilities/Camera.h"
#include <cmath>

Camera::Camera() {
	XMStoreFloat4x4(&m_view, XMMatrixIdentity());
	XMStoreFloat4x4(&m_proj, XMMatrixIdentity());
}

void 
Camera::setLens(float fovYRadians, 
								float aspectRatio, 
								float nearPlane, 
								float farPlane) {
	if (!std::isfinite(fovYRadians) || !std::isfinite(aspectRatio) ||
		!std::isfinite(nearPlane) || !std::isfinite(farPlane) ||
		fovYRadians <= 0.0f || fovYRadians >= XM_PI || aspectRatio <= 0.0f ||
		nearPlane <= 0.0f || farPlane <= nearPlane) {
		return;
	}
	m_fovY = fovYRadians;
	m_aspectRatio = aspectRatio;
	m_nearPlane = nearPlane;
	m_farPlane = farPlane;

	XMMATRIX proj = XMMatrixPerspectiveFovLH(m_fovY, m_aspectRatio, m_nearPlane, m_farPlane);
	XMStoreFloat4x4(&m_proj, proj);
}

void 
Camera::setPosition(float x, float y, float z) {
	if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
		return;
	}
	m_position = EU::Vector3(x, y, z);
	m_viewDirty = true;
}

void 
Camera::setPosition(const EU::Vector3& pos) {
	setPosition(pos.x, pos.y, pos.z);
}

void 
Camera::lookAt(const EU::Vector3& pos, const EU::Vector3& target, const EU::Vector3& up) {
	if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z) ||
		!std::isfinite(target.x) || !std::isfinite(target.y) || !std::isfinite(target.z) ||
		!std::isfinite(up.x) || !std::isfinite(up.y) || !std::isfinite(up.z)) {
		return;
	}
	auto lengthSq = [](const EU::Vector3& v) { return v.x * v.x + v.y * v.y + v.z * v.z; };
	auto normalizeSafe = [&](const EU::Vector3& v, const EU::Vector3& fallback) {
		const float lenSq = lengthSq(v);
		if (!std::isfinite(lenSq) || lenSq <= 1e-12f) {
			return fallback;
		}
		const float invLen = 1.0f / std::sqrt(lenSq);
		return EU::Vector3(v.x * invLen, v.y * invLen, v.z * invLen);
	};
	auto cross = [](const EU::Vector3& a, const EU::Vector3& b) {
		return EU::Vector3(
			a.y * b.z - a.z * b.y,
			a.z * b.x - a.x * b.z,
			a.x * b.y - a.y * b.x);
	};

	m_position = pos;
	EU::Vector3 forward = normalizeSafe(target - pos, EU::Vector3(0.0f, 0.0f, 1.0f));
	EU::Vector3 requestedUp = normalizeSafe(up, EU::Vector3(0.0f, 1.0f, 0.0f));
	EU::Vector3 right = cross(requestedUp, forward);
	if (lengthSq(right) <= 1e-12f) {
		// The requested up vector is parallel to forward. Choose a stable axis.
		requestedUp = std::fabs(forward.y) < 0.999f
			? EU::Vector3(0.0f, 1.0f, 0.0f)
			: EU::Vector3(1.0f, 0.0f, 0.0f);
		right = cross(requestedUp, forward);
	}
	right = normalizeSafe(right, EU::Vector3(1.0f, 0.0f, 0.0f));
	EU::Vector3 correctedUp = normalizeSafe(cross(forward, right), EU::Vector3(0.0f, 1.0f, 0.0f));

	m_forward = forward;
	m_right = right;
	m_up = correctedUp;
	m_viewDirty = true;
}

void 
Camera::walk(float d) {
	if (!std::isfinite(d)) return;
	XMVECTOR F = XMVectorSet(m_forward.x, m_forward.y, m_forward.z, 0.0f);
	XMVECTOR P = XMVectorSet(m_position.x, m_position.y, m_position.z, 1.0f);
	//P = XMVectorAdd(P, XMVectorScale(F, d));
	P += d * F;
	m_position = FromXM(P);
	m_viewDirty = true;
}

void 
Camera::strafe(float d) {
	if (!std::isfinite(d)) return;
	XMVECTOR R = XMVectorSet(m_right.x, m_right.y, m_right.z, 0.0f);
	XMVECTOR P = XMVectorSet(m_position.x, m_position.y, m_position.z, 1.0f);
	//P = XMVectorAdd(P, XMVectorScale(R, d));
	P += d * R;
	m_position = FromXM(P);
	m_viewDirty = true;
}

void 
Camera::yaw(float radians) {
	if (!std::isfinite(radians)) return;
	// Rotación alrededor del eje Y global
	XMMATRIX rot = XMMatrixRotationY(radians);

	XMVECTOR R = XMVectorSet(m_right.x, m_right.y, m_right.z, 0.0f);
	XMVECTOR U = XMVectorSet(m_up.x, m_up.y, m_up.z, 0.0f);
	XMVECTOR F = XMVectorSet(m_forward.x, m_forward.y, m_forward.z, 0.0f);

	R = XMVector3TransformNormal(R, rot);
	U = XMVector3TransformNormal(U, rot);
	F = XMVector3TransformNormal(F, rot);

	m_right = FromXM(R);
	m_up = FromXM(U);
	m_forward = FromXM(F);

	m_viewDirty = true;
}

void 
Camera::pitch(float radians) {
	if (!std::isfinite(radians)) return;
	// Rotación alrededor del eje Right local
	XMVECTOR R = XMVectorSet(m_right.x, m_right.y, m_right.z, 0.0f);
	XMMATRIX rot = XMMatrixRotationAxis(R, radians);

	XMVECTOR U = XMVectorSet(m_up.x, m_up.y, m_up.z, 0.0f);
	XMVECTOR F = XMVectorSet(m_forward.x, m_forward.y, m_forward.z, 0.0f);

	U = XMVector3TransformNormal(U, rot);
	F = XMVector3TransformNormal(F, rot);

	m_up = FromXM(U);
	m_forward = FromXM(F);

	m_viewDirty = true;
}

void 
Camera::updateViewMatrix() {
	if (!m_viewDirty) return;
	XMVECTOR R = XMVectorSet(m_right.x,			m_right.y,		m_right.z, 0.0f);
	XMVECTOR U = XMVectorSet(m_up.x,				m_up.y,				m_up.z, 0.0f);
	XMVECTOR F = XMVectorSet(m_forward.x,		m_forward.y,	m_forward.z, 0.0f);
	XMVECTOR P = XMVectorSet(m_position.x,	m_position.y, m_position.z, 1.0f);

	// Re-ortonormalizar (para evitar drift por floats). Protege contra
	// bases degeneradas para no propagar NaN a toda la matriz de vista.
	auto finiteLengthSq = [](const EU::Vector3& v) {
		const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
		return std::isfinite(lenSq) ? lenSq : 0.0f;
	};
	if (finiteLengthSq(m_forward) <= 1e-12f) m_forward = EU::Vector3(0.0f, 0.0f, 1.0f);
	if (finiteLengthSq(m_up) <= 1e-12f) m_up = EU::Vector3(0.0f, 1.0f, 0.0f);
	F = XMVectorSet(m_forward.x, m_forward.y, m_forward.z, 0.0f);
	U = XMVectorSet(m_up.x, m_up.y, m_up.z, 0.0f);
	F = XMVector3Normalize(F);
	R = XMVector3Cross(U, F);
	if (XMVectorGetX(XMVector3LengthSq(R)) <= 1e-8f) {
		U = (std::fabs(m_forward.y) < 0.999f)
			? XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
			: XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		R = XMVector3Cross(U, F);
	}
	R = XMVector3Normalize(R);
	U = XMVector3Normalize(XMVector3Cross(F, R));

	m_forward = FromXM(F);
	m_right = FromXM(R);
	m_up = FromXM(U);

	// View (LH) mirando en dirección forward | View = LookToLH(pos, pos + forward, up)
	XMMATRIX view = XMMatrixLookToLH(P, F, U);
	XMStoreFloat4x4(&m_view, view);
	m_viewDirty = false;
}


