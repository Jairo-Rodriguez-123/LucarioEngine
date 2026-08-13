#pragma once
#include "Prerequisites.h"
#include "Buffer.h"


struct
Submesh {
	Submesh() { XMStoreFloat4x4(&localTransform, XMMatrixIdentity()); }
	/*
	 *  @brief Vertex buffer containing the vertex attributes for this submesh.
	 */
	Buffer vertexBuffer;          
	/*
	 *  @brief Index buffer containing the indices used to draw this submesh.
	 */
	Buffer indexBuffer;           
	/*
	 *  @brief Number of indices to draw for this submesh.
	 */
	unsigned int indexCount = 0;  
	/*
	 *  @brief Starting index within the index buffer for this submesh.
	 */
	unsigned int startIndex = 0;  
	/*
	 *  @brief Material slot index used to select the material for this submesh.
	 */
	unsigned int materialSlot = 0;
	/* Local transform imported for this submesh. */
	XMFLOAT4X4 localTransform{};
};

class
Mesh {
public:
	/*
	 *  @brief Returns a modifiable reference to the vector of submeshes.
	 */
	std::vector<Submesh>& getSubmeshes() { return m_submeshes; }
	/*
	 *  @brief Returns a const reference to the vector of submeshes.
	 */
	const std::vector<Submesh>& getSubmeshes() const { return m_submeshes; }

	/*
	 *  @brief Destroys GPU resources (buffers) for all submeshes and clears the list.
	 */
	void
	destroy() {
		for (Submesh& submesh : m_submeshes) {
			submesh.vertexBuffer.destroy();
			submesh.indexBuffer.destroy();
		}
		m_submeshes.clear();
	}

private:
	/*
	 *  @brief Container holding all submeshes for this mesh.
	 */
	std::vector<Submesh> m_submeshes;
};


