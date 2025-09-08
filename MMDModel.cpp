#define GLM_ENABLE_EXPERIMENTAL

#include "MMDModel.h"

#include "PMXFile.h"
#include "VMDAnimation.h"

#include "Path.h"
#include "File.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <map>
#include <limits>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace saba
{
	MMDModel::MMDModel()
		: m_parallelUpdateCount(0)
	{
	}

	MMDModel::~MMDModel()
	{
		Destroy();
	}

	void MMDModel::InitializeAnimation()
	{
		ClearBaseAnimation();

		for (auto& node : m_nodeMan.m_nodes)
		{
			node->m_animTranslate = glm::vec3(0);
			node->m_animRotate = glm::quat(1, 0, 0, 0);
		}

		BeginAnimation();

		for (auto& node : m_nodeMan.m_nodes)
		{
			node->UpdateLocalTransform();
		}

		for (auto& morph : m_morphMan.m_morphs)
		{
			morph->m_weight = 0;
		}

		for (auto& ikSolver : m_ikSolverMan.m_ikSolvers)
		{
			ikSolver->m_enable = true;
		}

		for (const auto& node : m_nodeMan.m_nodes)
		{
			if (node->m_parent == nullptr)
			{
				node->UpdateGlobalTransform();
			}
		}

		for (auto pmxNode : m_sortedNodes)
		{
			if (pmxNode->m_appendNode != nullptr)
			{
				pmxNode->UpdateAppendTransform();
				pmxNode->UpdateGlobalTransform();
			}
			if (pmxNode->m_ikSolver != nullptr)
			{
				auto ikSolver = pmxNode->m_ikSolver;
				ikSolver->Solve();
				pmxNode->UpdateGlobalTransform();
			}
		}

		for (const auto& node : m_nodeMan.m_nodes)
		{
			if (node->m_parent == nullptr)
			{
				node->UpdateGlobalTransform();
			}
		}

		ResetPhysics();
	}

	void MMDModel::SaveBaseAnimation()
	{
		auto& nodeMan = m_nodeMan;
		for (size_t i = 0; i < nodeMan.m_nodes.size(); i++)
		{
			auto node = nodeMan.GetNode(i);
			node->SaveBaseAnimation();
		}

		auto& morphMan = m_morphMan;
		for (size_t i = 0; i < morphMan.m_morphs.size(); i++)
		{
			auto morph = morphMan.GetMorph(i);
			morph->SaveBaseAnimation();
		}

		auto& ikMan = m_ikSolverMan;
		for (size_t i = 0; i < ikMan.m_ikSolvers.size(); i++)
		{
			auto ikSolver = ikMan.GetIKSolver(i);
			ikSolver->SaveBaseAnimation();
		}
	}

	void MMDModel::LoadBaseAnimation()
	{
		auto& nodeMan = m_nodeMan;
		for (size_t i = 0; i < nodeMan.m_nodes.size(); i++)
		{
			auto node = nodeMan.GetNode(i);
			node->LoadBaseAnimation();
		}

		auto& morphMan = m_morphMan;
		for (size_t i = 0; i < morphMan.m_morphs.size(); i++)
		{
			auto morph = morphMan.GetMorph(i);
			morph->LoadBaseAnimation();
		}

		auto& ikMan = m_ikSolverMan;
		for (size_t i = 0; i < ikMan.m_ikSolvers.size(); i++)
		{
			auto ikSolver = ikMan.GetIKSolver(i);
			ikSolver->LoadBaseAnimation();
		}
	}

	void MMDModel::ClearBaseAnimation()
	{
		auto& nodeMan = m_nodeMan;
		for (size_t i = 0; i < nodeMan.m_nodes.size(); i++)
		{
			auto node = nodeMan.GetNode(i);
			node->ClearBaseAnimation();
		}

		auto& morphMan = m_morphMan;
		for (size_t i = 0; i < morphMan.m_morphs.size(); i++)
		{
			auto morph = morphMan.GetMorph(i);
			morph->ClearBaseAnimation();
		}

		auto& ikMan = m_ikSolverMan;
		for (size_t i = 0; i < ikMan.m_ikSolvers.size(); i++)
		{
			auto ikSolver = ikMan.GetIKSolver(i);
			ikSolver->ClearBaseAnimation();
		}
	}

	void MMDModel::BeginAnimation()
	{
		for (auto& node : m_nodeMan.m_nodes)
		{
			node->BeginUpdateTransform();
		}
		size_t vtxCount = m_morphPositions.size();
		for (size_t vtxIdx = 0; vtxIdx < vtxCount; vtxIdx++)
		{
			m_morphPositions[vtxIdx] = glm::vec3(0);
			m_morphUVs[vtxIdx] = glm::vec4(0);
		}
	}

	void MMDModel::UpdateMorphAnimation()
	{
		// Morph の処理
		BeginMorphMaterial();

		const auto& morphs = m_morphMan.m_morphs;
		for (size_t i = 0; i < morphs.size(); i++)
		{
			const auto& morph = morphs[i];
			Morph(morph.get(), morph->m_weight);
		}

		EndMorphMaterial();
	}

	namespace
	{
		glm::mat3 InvZ(const glm::mat3& m)
		{
			const glm::mat3 invZ = glm::scale(glm::mat4(1.0f), glm::vec3(1, 1, -1));
			return invZ * m * invZ;
		}
		glm::quat InvZ(const glm::quat& q)
		{
			auto rot0 = glm::mat3_cast(q);
			auto rot1 = InvZ(rot0);
			return glm::quat_cast(rot1);
		}
	}

	void MMDModel::UpdateNodeAnimation(bool afterPhysicsAnim)
	{
		for (auto pmxNode : m_sortedNodes)
		{
			if (pmxNode->m_isDeformAfterPhysics != afterPhysicsAnim)
			{
				continue;
			}

			pmxNode->UpdateLocalTransform();
		}

		for (auto pmxNode : m_sortedNodes)
		{
			if (pmxNode->m_isDeformAfterPhysics != afterPhysicsAnim)
			{
				continue;
			}

			if (pmxNode->m_parent == nullptr)
			{
				pmxNode->UpdateGlobalTransform();
			}
		}

		for (auto pmxNode : m_sortedNodes)
		{
			if (pmxNode->m_isDeformAfterPhysics != afterPhysicsAnim)
			{
				continue;
			}

			if (pmxNode->m_appendNode != nullptr)
			{
				pmxNode->UpdateAppendTransform();
				pmxNode->UpdateGlobalTransform();
			}
			if (pmxNode->m_ikSolver != nullptr)
			{
				auto ikSolver = pmxNode->m_ikSolver;
				ikSolver->Solve();
				pmxNode->UpdateGlobalTransform();
			}
		}

		for (auto pmxNode : m_sortedNodes)
		{
			if (pmxNode->m_isDeformAfterPhysics != afterPhysicsAnim)
			{
				continue;
			}

			if (pmxNode->m_parent == nullptr)
			{
				pmxNode->UpdateGlobalTransform();
			}
		}
	}

	void MMDModel::ResetPhysics()
	{
		MMDPhysicsManager& physicsMan = m_physicsMan;
		auto physics = physicsMan.GetMMDPhysics();

		if (physics == nullptr)
		{
			return;
		}

		auto& rigidbodys = physicsMan.m_rigidBodys;
		for (auto& rb : rigidbodys)
		{
			rb->SetActivation(false);
			rb->ResetTransform();
		}

		physics->Update(1.0f / 60.0f);

		for (auto& rb : rigidbodys)
		{
			rb->ReflectGlobalTransform();
		}

		for (auto& rb : rigidbodys)
		{
			rb->CalcLocalTransform();
		}

		for (const auto& node : m_nodeMan.m_nodes)
		{
			if (node->m_parent == nullptr)
			{
				node->UpdateGlobalTransform();
			}
		}

		for (auto& rb : rigidbodys)
		{
			rb->Reset(physics);
		}
	}

	void MMDModel::UpdatePhysicsAnimation(float elapsed)
	{
		MMDPhysicsManager& physicsMan = m_physicsMan;
		auto physics = physicsMan.GetMMDPhysics();

		if (physics == nullptr)
		{
			return;
		}

		auto& rigidbodys = physicsMan.m_rigidBodys;
		for (auto& rb : rigidbodys)
		{
			rb->SetActivation(true);
		}

		physics->Update(elapsed);

		for (auto& rb : rigidbodys)
		{
			rb->ReflectGlobalTransform();
		}

		for (auto& rb : rigidbodys)
		{
			rb->CalcLocalTransform();
		}

		for (const auto& node : m_nodeMan.m_nodes)
		{
			if (node->m_parent == nullptr)
			{
				node->UpdateGlobalTransform();
			}
		}
	}

	void MMDModel::Update()
	{
		auto& nodes = m_nodeMan.m_nodes;

		// スキンメッシュに使用する変形マトリクスを事前計算
		for (size_t i = 0; i < nodes.size(); i++)
		{
			m_transforms[i] = nodes[i]->m_global * nodes[i]->m_inverseInit;
		}

		if (m_parallelUpdateCount != m_updateRanges.size())
		{
			SetupParallelUpdate();
		}

		size_t futureCount = m_parallelUpdateFutures.size();
		for (size_t i = 0; i < futureCount; i++)
		{
			size_t rangeIndex = i + 1;
			if (m_updateRanges[rangeIndex].m_vertexCount != 0)
			{
				m_parallelUpdateFutures[i] = std::async(
					std::launch::async,
					[this, rangeIndex]() { this->Update(this->m_updateRanges[rangeIndex]); }
				);
			}
		}

		Update(m_updateRanges[0]);

		for (size_t i = 0; i < futureCount; i++)
		{
			size_t rangeIndex = i + 1;
			if (m_updateRanges[rangeIndex].m_vertexCount != 0)
			{
				m_parallelUpdateFutures[i].wait();
			}
		}
	}

	void MMDModel::UpdateAllAnimation(VMDAnimation* vmdAnim, float vmdFrame, float physicsElapsed)
	{
		if (vmdAnim != nullptr)
		{
			vmdAnim->Evaluate(vmdFrame);
		}

		UpdateMorphAnimation();

		UpdateNodeAnimation(false);

		UpdatePhysicsAnimation(physicsElapsed);

		UpdateNodeAnimation(true);
	}

	bool MMDModel::Load(const std::string& filepath, const std::string& mmdDataDir)
	{
		Destroy();

		PMXFile pmx;
		if (!ReadPMXFile(&pmx, filepath.c_str()))
		{
			return false;
		}

		std::string dirPath = PathUtil::GetDirectoryName(filepath);

		size_t vertexCount = pmx.m_vertices.size();
		m_positions.reserve(vertexCount);
		m_normals.reserve(vertexCount);
		m_uvs.reserve(vertexCount);
		m_vertexBoneInfos.reserve(vertexCount);
		m_bboxMax = glm::vec3(-std::numeric_limits<float>::max());
		m_bboxMin = glm::vec3(std::numeric_limits<float>::max());

		bool warnSDEF = false;
		bool infoQDEF = false;
		for (const auto& v : pmx.m_vertices)
		{
			glm::vec3 pos = v.m_position * glm::vec3(1, 1, -1);
			glm::vec3 nor = v.m_normal * glm::vec3(1, 1, -1);
			glm::vec2 uv = glm::vec2(v.m_uv.x, 1.0f - v.m_uv.y);
			m_positions.push_back(pos);
			m_normals.push_back(nor);
			m_uvs.push_back(uv);
			VertexBoneInfo vtxBoneInfo;
			if (PMXVertexWeight::SDEF != v.m_weightType)
			{
				vtxBoneInfo.m_boneIndex[0] = v.m_boneIndices[0];
				vtxBoneInfo.m_boneIndex[1] = v.m_boneIndices[1];
				vtxBoneInfo.m_boneIndex[2] = v.m_boneIndices[2];
				vtxBoneInfo.m_boneIndex[3] = v.m_boneIndices[3];

				vtxBoneInfo.m_boneWeight[0] = v.m_boneWeights[0];
				vtxBoneInfo.m_boneWeight[1] = v.m_boneWeights[1];
				vtxBoneInfo.m_boneWeight[2] = v.m_boneWeights[2];
				vtxBoneInfo.m_boneWeight[3] = v.m_boneWeights[3];
			}

			switch (v.m_weightType)
			{
			case PMXVertexWeight::BDEF1:
				vtxBoneInfo.m_skinningType = SkinningType::Weight1;
				break;
			case PMXVertexWeight::BDEF2:
				vtxBoneInfo.m_skinningType = SkinningType::Weight2;
				vtxBoneInfo.m_boneWeight[1] = 1.0f - vtxBoneInfo.m_boneWeight[0];
				break;
			case PMXVertexWeight::BDEF4:
				vtxBoneInfo.m_skinningType = SkinningType::Weight4;
				break;
			case PMXVertexWeight::SDEF:
				if (!warnSDEF)
				{
					warnSDEF = true;
				}
				vtxBoneInfo.m_skinningType = SkinningType::SDEF;
				{
					auto i0 = v.m_boneIndices[0];
					auto i1 = v.m_boneIndices[1];
					auto w0 = v.m_boneWeights[0];
					auto w1 = 1.0f - w0;

					auto center = v.m_sdefC * glm::vec3(1, 1, -1);
					auto r0 = v.m_sdefR0 * glm::vec3(1, 1, -1);
					auto r1 = v.m_sdefR1 * glm::vec3(1, 1, -1);
					auto rw = r0 * w0 + r1 * w1;
					r0 = center + r0 - rw;
					r1 = center + r1 - rw;
					auto cr0 = (center + r0) * 0.5f;
					auto cr1 = (center + r1) * 0.5f;

					vtxBoneInfo.m_sdef.m_boneIndex[0] = v.m_boneIndices[0];
					vtxBoneInfo.m_sdef.m_boneIndex[1] = v.m_boneIndices[1];
					vtxBoneInfo.m_sdef.m_boneWeight = v.m_boneWeights[0];
					vtxBoneInfo.m_sdef.m_sdefC = center;
					vtxBoneInfo.m_sdef.m_sdefR0 = cr0;
					vtxBoneInfo.m_sdef.m_sdefR1 = cr1;
				}
				break;
			case PMXVertexWeight::QDEF:
				vtxBoneInfo.m_skinningType = SkinningType::DualQuaternion;
				if (!infoQDEF)
				{
					infoQDEF = true;
				}
				break;
			default:
				vtxBoneInfo.m_skinningType = SkinningType::Weight1;
				break;
			}
			m_vertexBoneInfos.push_back(vtxBoneInfo);

			m_bboxMax = glm::max(m_bboxMax, pos);
			m_bboxMin = glm::min(m_bboxMin, pos);
		}
		m_morphPositions.resize(m_positions.size());
		m_morphUVs.resize(m_positions.size());
		m_updatePositions.resize(m_positions.size());
		m_updateNormals.resize(m_normals.size());
		m_updateUVs.resize(m_uvs.size());


		m_indexElementSize = pmx.m_header.m_vertexIndexSize;
		m_indices.resize(pmx.m_faces.size() * 3 * m_indexElementSize);
		m_indexCount = pmx.m_faces.size() * 3;
		switch (m_indexElementSize)
		{
		case 1:
		{
			int idx = 0;
			uint8_t* indices = (uint8_t*)m_indices.data();
			for (const auto& face : pmx.m_faces)
			{
				for (int i = 0; i < 3; i++)
				{
					auto vi = face.m_vertices[3 - i - 1];
					indices[idx] = (uint8_t)vi;
					idx++;
				}
			}
			break;
		}
		case 2:
		{
			int idx = 0;
			uint16_t* indices = (uint16_t*)m_indices.data();
			for (const auto& face : pmx.m_faces)
			{
				for (int i = 0; i < 3; i++)
				{
					auto vi = face.m_vertices[3 - i - 1];
					indices[idx] = (uint16_t)vi;
					idx++;
				}
			}
			break;
		}
		case 4:
		{
			int idx = 0;
			uint32_t* indices = (uint32_t*)m_indices.data();
			for (const auto& face : pmx.m_faces)
			{
				for (int i = 0; i < 3; i++)
				{
					auto vi = face.m_vertices[3 - i - 1];
					indices[idx] = (uint32_t)vi;
					idx++;
				}
			}
			break;
		}
		default:
			return false;
		}

		std::vector<std::string> texturePaths;
		texturePaths.reserve(pmx.m_textures.size());
		for (const auto& pmxTex : pmx.m_textures)
		{
			std::string texPath = PathUtil::Combine(dirPath, pmxTex.m_textureName);
			texturePaths.emplace_back(std::move(texPath));
		}

		// Materialをコピー
		m_materials.reserve(pmx.m_materials.size());
		m_subMeshes.reserve(pmx.m_materials.size());
		uint32_t beginIndex = 0;
		for (const auto& pmxMat : pmx.m_materials)
		{
			MMDMaterial mat;
			mat.m_diffuse = pmxMat.m_diffuse;
			mat.m_alpha = pmxMat.m_diffuse.a;
			mat.m_specularPower = pmxMat.m_specularPower;
			mat.m_specular = pmxMat.m_specular;
			mat.m_ambient = pmxMat.m_ambient;
			mat.m_spTextureMode = MMDMaterial::SphereTextureMode::None;
			mat.m_bothFace = !!((uint8_t)pmxMat.m_drawMode & (uint8_t)PMXDrawModeFlags::BothFace);
			mat.m_edgeFlag = ((uint8_t)pmxMat.m_drawMode & (uint8_t)PMXDrawModeFlags::DrawEdge) == 0 ? 0 : 1;
			mat.m_groundShadow = !!((uint8_t)pmxMat.m_drawMode & (uint8_t)PMXDrawModeFlags::GroundShadow);
			mat.m_shadowCaster = !!((uint8_t)pmxMat.m_drawMode & (uint8_t)PMXDrawModeFlags::CastSelfShadow);
			mat.m_shadowReceiver = !!((uint8_t)pmxMat.m_drawMode & (uint8_t)PMXDrawModeFlags::RecieveSelfShadow);
			mat.m_edgeSize = pmxMat.m_edgeSize;
			mat.m_edgeColor = pmxMat.m_edgeColor;

			// Texture
			if (pmxMat.m_textureIndex != -1)
			{
				mat.m_texture = PathUtil::Normalize(texturePaths[pmxMat.m_textureIndex]);
			}

			// ToonTexture
			if (pmxMat.m_toonMode == PMXToonMode::Common)
			{
				if (pmxMat.m_toonTextureIndex != -1)
				{
					std::stringstream ss;
					ss << "toon" << std::setfill('0') << std::setw(2) << (pmxMat.m_toonTextureIndex + 1) << ".bmp";
					mat.m_toonTexture = PathUtil::Combine(mmdDataDir, ss.str());
				}
			}
			else if (pmxMat.m_toonMode == PMXToonMode::Separate)
			{
				if (pmxMat.m_toonTextureIndex != -1)
				{
					mat.m_toonTexture = PathUtil::Normalize(texturePaths[pmxMat.m_toonTextureIndex]);
				}
			}

			// SpTexture
			if (pmxMat.m_sphereTextureIndex != -1)
			{
				mat.m_spTexture = PathUtil::Normalize(texturePaths[pmxMat.m_sphereTextureIndex]);
				mat.m_spTextureMode = MMDMaterial::SphereTextureMode::None;
				if (pmxMat.m_sphereMode == PMXSphereMode::Mul)
				{
					mat.m_spTextureMode = MMDMaterial::SphereTextureMode::Mul;
				}
				else if (pmxMat.m_sphereMode == PMXSphereMode::Add)
				{
					mat.m_spTextureMode = MMDMaterial::SphereTextureMode::Add;
				}
				else if (pmxMat.m_sphereMode == PMXSphereMode::SubTexture)
				{
					// TODO: SphareTexture が SubTexture の処理
				}
			}

			m_materials.emplace_back(std::move(mat));

			MMDSubMesh subMesh;
			subMesh.m_beginIndex = beginIndex;
			subMesh.m_vertexCount = pmxMat.m_numFaceVertices;
			subMesh.m_materialID = (int)(m_materials.size() - 1);
			m_subMeshes.push_back(subMesh);

			beginIndex = beginIndex + pmxMat.m_numFaceVertices;
		}
		m_initMaterials = m_materials;
		m_mulMaterialFactors.resize(m_materials.size());
		m_addMaterialFactors.resize(m_materials.size());

		// Node
		m_nodeMan.m_nodes.reserve(pmx.m_bones.size());
		for (const auto& bone : pmx.m_bones)
		{
			auto* node = m_nodeMan.AddNode();
			node->m_name = bone.m_name;
		}
		for (size_t i = 0; i < pmx.m_bones.size(); i++)
		{
			const auto& bone = pmx.m_bones[i];
			auto* node = m_nodeMan.GetNode(i);
			if (bone.m_parentBoneIndex != -1)
			{
				const auto& parentBone = pmx.m_bones[bone.m_parentBoneIndex];
				auto* parent = m_nodeMan.GetNode(bone.m_parentBoneIndex);
				parent->AddChild(node);
				auto localPos = bone.m_position - parentBone.m_position;
				localPos.z *= -1;
				node->m_translate = localPos;
			}
			else
			{
				auto localPos = bone.m_position;
				localPos.z *= -1;
				node->m_translate = localPos;
			}
			glm::mat4 init = glm::translate(
				glm::mat4(1),
				bone.m_position * glm::vec3(1, 1, -1)
			);
			node->m_global = init;
			node->CalculateInverseInitTransform();

			node->m_deformDepth = bone.m_deformDepth;
			bool deformAfterPhysics = !!((uint16_t)bone.m_boneFlag & (uint16_t)PMXBoneFlags::DeformAfterPhysics);
			node->m_isDeformAfterPhysics = deformAfterPhysics;
			bool appendRotate = ((uint16_t)bone.m_boneFlag & (uint16_t)PMXBoneFlags::AppendRotate) != 0;
			bool appendTranslate = ((uint16_t)bone.m_boneFlag & (uint16_t)PMXBoneFlags::AppendTranslate) != 0;
			node->m_isAppendRotate = appendRotate;
			node->m_isAppendTranslate = appendTranslate;
			if ((appendRotate || appendTranslate) && (bone.m_appendBoneIndex != -1))
			{
				bool appendLocal = ((uint16_t)bone.m_boneFlag & (uint16_t)PMXBoneFlags::AppendLocal) != 0;
				auto appendNode = m_nodeMan.GetNode(bone.m_appendBoneIndex);
				float appendWeight = bone.m_appendWeight;
				node->m_isAppendLocal = appendLocal;
				node->m_appendNode = appendNode;
				node->m_appendWeight = appendWeight;
			}
			node->SaveInitialTRS();
		}
		m_transforms.resize(m_nodeMan.m_nodes.size());

		m_sortedNodes.clear();
		m_sortedNodes.reserve(m_nodeMan.m_nodes.size());
		auto& pmxNodes = m_nodeMan.m_nodes;
		for (auto& pmxNode : pmxNodes)
		{
			m_sortedNodes.push_back(pmxNode.get());
		}
		std::stable_sort(
			m_sortedNodes.begin(),
			m_sortedNodes.end(),
			[](const MMDNode* x, const MMDNode* y) {return x->m_deformDepth < y->m_deformDepth; }
		);

		// IK
		for (size_t i = 0; i < pmx.m_bones.size(); i++)
		{
			const auto& bone = pmx.m_bones[i];
			if ((uint16_t)bone.m_boneFlag & (uint16_t)PMXBoneFlags::IK)
			{
				auto solver = m_ikSolverMan.AddIKSolver();
				auto* ikNode = m_nodeMan.GetNode(i);
				solver->m_ikNode = ikNode;
				ikNode->m_ikSolver = solver;

				auto* targetNode = m_nodeMan.GetNode(bone.m_ikTargetBoneIndex);
				solver->m_ikTarget = targetNode;

				for (const auto& ikLink : bone.m_ikLinks)
				{
					auto* linkNode = m_nodeMan.GetNode(ikLink.m_ikBoneIndex);
					if (ikLink.m_enableLimit)
					{
						glm::vec3 limitMax = ikLink.m_limitMin * glm::vec3(-1);
						glm::vec3 limitMin = ikLink.m_limitMax * glm::vec3(-1);
						solver->AddIKChain(linkNode, true, limitMin, limitMax);
					}
					else
					{
						solver->AddIKChain(linkNode);
					}
					linkNode->m_enableIK = true;
				}

				solver->m_iterateCount = bone.m_ikIterationCount;
				solver->m_limitAngle = bone.m_ikLimit;
			}
		}

		// Morph
		for (const auto& pmxMorph : pmx.m_morphs)
		{
			auto morph = m_morphMan.AddMorph();
			morph->m_name = pmxMorph.m_name;
			morph->m_weight = 0.0f;
			morph->m_morphType = MorphType::None;
			if (pmxMorph.m_morphType == PMXMorphType::Position)
			{
				morph->m_morphType = MorphType::Position;
				morph->m_dataIndex = m_positionMorphDatas.size();
				PositionMorphData morphData;
				for (const auto& vtx : pmxMorph.m_positionMorph)
				{
					PositionMorph morphVtx;
					morphVtx.m_index = vtx.m_vertexIndex;
					morphVtx.m_position = vtx.m_position * glm::vec3(1, 1, -1);
					morphData.m_morphVertices.push_back(morphVtx);
				}
				m_positionMorphDatas.emplace_back(std::move(morphData));
			}
			else if (pmxMorph.m_morphType == PMXMorphType::UV)
			{
				morph->m_morphType = MorphType::UV;
				morph->m_dataIndex = m_uvMorphDatas.size();
				UVMorphData morphData;
				for (const auto& uv : pmxMorph.m_uvMorph)
				{
					UVMorph morphUV;
					morphUV.m_index = uv.m_vertexIndex;
					morphUV.m_uv = uv.m_uv;
					morphData.m_morphUVs.push_back(morphUV);
				}
				m_uvMorphDatas.emplace_back(std::move(morphData));
			}
			else if (pmxMorph.m_morphType == PMXMorphType::Material)
			{
				morph->m_morphType = MorphType::Material;
				morph->m_dataIndex = m_materialMorphDatas.size();

				MaterialMorphData materialMorphData;
				materialMorphData.m_materialMorphs = pmxMorph.m_materialMorph;
				m_materialMorphDatas.emplace_back(materialMorphData);
			}
			else if (pmxMorph.m_morphType == PMXMorphType::Bone)
			{
				morph->m_morphType = MorphType::Bone;
				morph->m_dataIndex = m_boneMorphDatas.size();

				BoneMorphData boneMorphData;
				for (const auto& pmxBoneMorphElem : pmxMorph.m_boneMorph)
				{
					BoneMorphElement boneMorphElem;
					boneMorphElem.m_node = m_nodeMan.GetNode(pmxBoneMorphElem.m_boneIndex);
					boneMorphElem.m_position = pmxBoneMorphElem.m_position * glm::vec3(1, 1, -1);
					const glm::quat q = pmxBoneMorphElem.m_quaternion;
					auto invZ = glm::mat3(glm::scale(glm::mat4(1), glm::vec3(1, 1, -1)));
					auto rot0 = glm::mat3_cast(q);
					auto rot1 = invZ * rot0 * invZ;
					boneMorphElem.m_rotate = glm::quat_cast(rot1);
					boneMorphData.m_boneMorphs.push_back(boneMorphElem);
				}
				m_boneMorphDatas.emplace_back(boneMorphData);
			}
			else if (pmxMorph.m_morphType == PMXMorphType::Group)
			{
				morph->m_morphType = MorphType::Group;
				morph->m_dataIndex = m_groupMorphDatas.size();

				GroupMorphData groupMorphData;
				groupMorphData.m_groupMorphs = pmxMorph.m_groupMorph;
				m_groupMorphDatas.emplace_back(groupMorphData);
			}
		}

		// Check whether Group Morph infinite loop.
		{
			std::vector<int32_t> groupMorphStack;
			std::function<void(int32_t)> fixInifinitGropuMorph;
			fixInifinitGropuMorph = [this, &fixInifinitGropuMorph, &groupMorphStack](int32_t morphIdx)
				{
					const auto& morphs = m_morphMan.m_morphs;
					const auto& morph = morphs[morphIdx];

					if (morph->m_morphType == MorphType::Group)
					{
						auto& groupMorphData = m_groupMorphDatas[morph->m_dataIndex];
						for (size_t i = 0; i < groupMorphData.m_groupMorphs.size(); i++)
						{
							auto& groupMorph = groupMorphData.m_groupMorphs[i];

							auto findIt = std::find(
								groupMorphStack.begin(),
								groupMorphStack.end(),
								groupMorph.m_morphIndex
							);
							if (findIt != groupMorphStack.end())
							{
								groupMorph.m_morphIndex = -1;
							}
							else
							{
								groupMorphStack.push_back(morphIdx);
								fixInifinitGropuMorph(groupMorph.m_morphIndex);
								groupMorphStack.pop_back();
							}
						}
					}
				};

			for (int32_t morphIdx = 0; morphIdx < int32_t(m_morphMan.m_morphs.size()); morphIdx++)
			{
				fixInifinitGropuMorph(morphIdx);
				groupMorphStack.clear();
			}

		}

		// Physics
		if (!m_physicsMan.Create())
		{
			return false;
		}

		for (const auto& pmxRB : pmx.m_rigidbodies)
		{
			auto rb = m_physicsMan.AddRigidBody();
			MMDNode* node = nullptr;
			if (pmxRB.m_boneIndex != -1)
			{
				node = m_nodeMan.GetNode(pmxRB.m_boneIndex);
			}
			if (!rb->Create(pmxRB, this, node))
			{
				return false;
			}
			m_physicsMan.GetMMDPhysics()->AddRigidBody(rb);
		}

		for (const auto& pmxJoint : pmx.m_joints)
		{
			if (pmxJoint.m_rigidbodyAIndex != -1 &&
				pmxJoint.m_rigidbodyBIndex != -1 &&
				pmxJoint.m_rigidbodyAIndex != pmxJoint.m_rigidbodyBIndex)
			{
				auto joint = m_physicsMan.AddJoint();
				MMDNode* node = nullptr;
				auto& rigidBodys = m_physicsMan.m_rigidBodys;
				bool ret = joint->CreateJoint(
					pmxJoint,
					rigidBodys[pmxJoint.m_rigidbodyAIndex].get(),
					rigidBodys[pmxJoint.m_rigidbodyBIndex].get()
				);
				if (!ret)
				{
					return false;
				}
				m_physicsMan.GetMMDPhysics()->AddJoint(joint);
			}
		}

		ResetPhysics();

		SetupParallelUpdate();

		return true;
	}

	void MMDModel::Destroy()
	{
		m_materials.clear();
		m_subMeshes.clear();

		m_positions.clear();
		m_normals.clear();
		m_uvs.clear();
		m_vertexBoneInfos.clear();

		m_indices.clear();

		m_nodeMan.m_nodes.clear();

		m_updateRanges.clear();
	}

	void MMDModel::SetupParallelUpdate()
	{
		if (m_parallelUpdateCount == 0)
		{
			m_parallelUpdateCount = std::thread::hardware_concurrency();
		}
		size_t maxParallelCount = std::max(size_t(16), size_t(std::thread::hardware_concurrency()));
		if (m_parallelUpdateCount > maxParallelCount)
		{
			m_parallelUpdateCount = 16;
		}

		m_updateRanges.resize(m_parallelUpdateCount);
		m_parallelUpdateFutures.resize(m_parallelUpdateCount - 1);

		const size_t vertexCount = m_positions.size();
		const size_t LowerVertexCount = 1000;
		if (vertexCount < m_updateRanges.size() * LowerVertexCount)
		{
			size_t numRanges = (vertexCount + LowerVertexCount - 1) / LowerVertexCount;
			for (size_t rangeIdx = 0; rangeIdx < m_updateRanges.size(); rangeIdx++)
			{
				auto& range = m_updateRanges[rangeIdx];
				if (rangeIdx < numRanges)
				{
					range.m_vertexOffset = rangeIdx * LowerVertexCount;
					range.m_vertexCount = std::min(LowerVertexCount, vertexCount - range.m_vertexOffset);
				}
				else
				{
					range.m_vertexOffset = 0;
					range.m_vertexCount = 0;
				}
			}
		}
		else
		{
			size_t numVertexCount = vertexCount / m_updateRanges.size();
			size_t offset = 0;
			for (size_t rangeIdx = 0; rangeIdx < m_updateRanges.size(); rangeIdx++)
			{
				auto& range = m_updateRanges[rangeIdx];
				range.m_vertexOffset = offset;
				range.m_vertexCount = numVertexCount;
				if (rangeIdx == 0)
				{
					range.m_vertexCount += vertexCount % m_updateRanges.size();
				}
				offset = range.m_vertexOffset + range.m_vertexCount;
			}
		}
	}

	void MMDModel::Update(const UpdateRange& range)
	{
		const auto* position = m_positions.data() + range.m_vertexOffset;
		const auto* normal = m_normals.data() + range.m_vertexOffset;
		const auto* uv = m_uvs.data() + range.m_vertexOffset;
		const auto* morphPos = m_morphPositions.data() + range.m_vertexOffset;
		const auto* morphUV = m_morphUVs.data() + range.m_vertexOffset;
		const auto* vtxInfo = m_vertexBoneInfos.data() + range.m_vertexOffset;
		const auto* transforms = m_transforms.data();
		auto* updatePosition = m_updatePositions.data() + range.m_vertexOffset;
		auto* updateNormal = m_updateNormals.data() + range.m_vertexOffset;
		auto* updateUV = m_updateUVs.data() + range.m_vertexOffset;

		for (size_t i = 0; i < range.m_vertexCount; i++)
		{
			glm::mat4 m;
			switch (vtxInfo->m_skinningType)
			{
			case MMDModel::SkinningType::Weight1:
			{
				const auto i0 = vtxInfo->m_boneIndex[0];
				const auto& m0 = transforms[i0];
				m = m0;
				break;
			}
			case MMDModel::SkinningType::Weight2:
			{
				const auto i0 = vtxInfo->m_boneIndex[0];
				const auto i1 = vtxInfo->m_boneIndex[1];
				const auto w0 = vtxInfo->m_boneWeight[0];
				const auto w1 = vtxInfo->m_boneWeight[1];
				const auto& m0 = transforms[i0];
				const auto& m1 = transforms[i1];
				m = m0 * w0 + m1 * w1;
				break;
			}
			case MMDModel::SkinningType::Weight4:
			{
				const auto i0 = vtxInfo->m_boneIndex[0];
				const auto i1 = vtxInfo->m_boneIndex[1];
				const auto i2 = vtxInfo->m_boneIndex[2];
				const auto i3 = vtxInfo->m_boneIndex[3];
				const auto w0 = vtxInfo->m_boneWeight[0];
				const auto w1 = vtxInfo->m_boneWeight[1];
				const auto w2 = vtxInfo->m_boneWeight[2];
				const auto w3 = vtxInfo->m_boneWeight[3];
				const auto& m0 = transforms[i0];
				const auto& m1 = transforms[i1];
				const auto& m2 = transforms[i2];
				const auto& m3 = transforms[i3];
				m = m0 * w0 + m1 * w1 + m2 * w2 + m3 * w3;
				break;
			}
			case MMDModel::SkinningType::SDEF:
			{
				// https://github.com/powroupi/blender_mmd_tools/blob/dev_test/mmd_tools/core/sdef.py

				auto& nodes = m_nodeMan.m_nodes;
				const auto i0 = vtxInfo->m_sdef.m_boneIndex[0];
				const auto i1 = vtxInfo->m_sdef.m_boneIndex[1];
				const auto w0 = vtxInfo->m_sdef.m_boneWeight;
				const auto w1 = 1.0f - w0;
				const auto center = vtxInfo->m_sdef.m_sdefC;
				const auto cr0 = vtxInfo->m_sdef.m_sdefR0;
				const auto cr1 = vtxInfo->m_sdef.m_sdefR1;
				const auto q0 = glm::quat_cast(nodes[i0]->m_global);
				const auto q1 = glm::quat_cast(nodes[i1]->m_global);
				const auto m0 = transforms[i0];
				const auto m1 = transforms[i1];

				const auto pos = *position + *morphPos;
				const auto rot_mat = glm::mat3_cast(glm::slerp(q0, q1, w1));

				*updatePosition = glm::mat3(rot_mat) * (pos - center) + glm::vec3(m0 * glm::vec4(cr0, 1)) * w0 + glm::vec3(m1 * glm::vec4(cr1, 1)) * w1;
				*updateNormal = rot_mat * *normal;

				break;
			}
			case MMDModel::SkinningType::DualQuaternion:
			{
				//
				// Skinning with Dual Quaternions
				// https://www.cs.utah.edu/~ladislav/dq/index.html
				//
				glm::dualquat dq[4];
				float w[4] = { 0 };
				for (int bi = 0; bi < 4; bi++)
				{
					auto boneID = vtxInfo->m_boneIndex[bi];
					if (boneID != -1)
					{
						dq[bi] = glm::dualquat_cast(glm::mat3x4(glm::transpose(transforms[boneID])));
						dq[bi] = glm::normalize(dq[bi]);
						w[bi] = vtxInfo->m_boneWeight[bi];
					}
					else
					{
						w[bi] = 0;
					}
				}
				if (glm::dot(dq[0].real, dq[1].real) < 0) { w[1] *= -1.0f; }
				if (glm::dot(dq[0].real, dq[2].real) < 0) { w[2] *= -1.0f; }
				if (glm::dot(dq[0].real, dq[3].real) < 0) { w[3] *= -1.0f; }
				auto blendDQ = w[0] * dq[0]
					+ w[1] * dq[1]
					+ w[2] * dq[2]
					+ w[3] * dq[3];
				blendDQ = glm::normalize(blendDQ);
				m = glm::transpose(glm::mat3x4_cast(blendDQ));
				break;
			}
			default:
				break;
			}

			if (MMDModel::SkinningType::SDEF != vtxInfo->m_skinningType)
			{
				*updatePosition = glm::vec3(m * glm::vec4(*position + *morphPos, 1));
				*updateNormal = glm::normalize(glm::mat3(m) * *normal);
			}
			*updateUV = *uv + glm::vec2((*morphUV).x, (*morphUV).y);

			vtxInfo++;
			position++;
			normal++;
			uv++;
			updatePosition++;
			updateNormal++;
			updateUV++;
			morphPos++;
			morphUV++;
		}
	}

	void MMDModel::Morph(MMDMorph* morph, float weight)
	{
		switch (morph->m_morphType)
		{
		case MorphType::Position:
			MorphPosition(
				m_positionMorphDatas[morph->m_dataIndex],
				weight
			);
			break;
		case MorphType::UV:
			MorphUV(
				m_uvMorphDatas[morph->m_dataIndex],
				weight
			);
			break;
		case MorphType::Material:
			MorphMaterial(
				m_materialMorphDatas[morph->m_dataIndex],
				weight
			);
			break;
		case MorphType::Bone:
			MorphBone(
				m_boneMorphDatas[morph->m_dataIndex],
				weight
			);
			break;
		case MorphType::Group:
		{
			auto& groupMorphData = m_groupMorphDatas[morph->m_dataIndex];
			for (const auto& groupMorph : groupMorphData.m_groupMorphs)
			{
				if (groupMorph.m_morphIndex == -1) { continue; }
				auto& elemMorph = m_morphMan.m_morphs[groupMorph.m_morphIndex];
				Morph(elemMorph.get(), groupMorph.m_weight * weight);
			}
			break;
		}
		default:
			break;
		}
	}

	void MMDModel::MorphPosition(const PositionMorphData& morphData, float weight)
	{
		if (weight == 0)
		{
			return;
		}

		for (const auto& morphVtx : morphData.m_morphVertices)
		{
			m_morphPositions[morphVtx.m_index] += morphVtx.m_position * weight;
		}
	}

	void MMDModel::MorphUV(const UVMorphData& morphData, float weight)
	{
		if (weight == 0)
		{
			return;
		}

		for (const auto& morphUV : morphData.m_morphUVs)
		{
			m_morphUVs[morphUV.m_index] += morphUV.m_uv * weight;
		}
	}

	void MMDModel::BeginMorphMaterial()
	{
		MaterialFactor initMul;
		initMul.m_diffuse = glm::vec3(1);
		initMul.m_alpha = 1;
		initMul.m_specular = glm::vec3(1);
		initMul.m_specularPower = 1;
		initMul.m_ambient = glm::vec3(1);
		initMul.m_edgeColor = glm::vec4(1);
		initMul.m_edgeSize = 1;
		initMul.m_textureFactor = glm::vec4(1);
		initMul.m_spTextureFactor = glm::vec4(1);
		initMul.m_toonTextureFactor = glm::vec4(1);

		MaterialFactor initAdd;
		initAdd.m_diffuse = glm::vec3(0);
		initAdd.m_alpha = 0;
		initAdd.m_specular = glm::vec3(0);
		initAdd.m_specularPower = 0;
		initAdd.m_ambient = glm::vec3(0);
		initAdd.m_edgeColor = glm::vec4(0);
		initAdd.m_edgeSize = 0;
		initAdd.m_textureFactor = glm::vec4(0);
		initAdd.m_spTextureFactor = glm::vec4(0);
		initAdd.m_toonTextureFactor = glm::vec4(0);

		size_t matCount = m_materials.size();
		for (size_t matIdx = 0; matIdx < matCount; matIdx++)
		{
			m_mulMaterialFactors[matIdx] = initMul;
			m_mulMaterialFactors[matIdx].m_diffuse = m_initMaterials[matIdx].m_diffuse;
			m_mulMaterialFactors[matIdx].m_alpha = m_initMaterials[matIdx].m_alpha;
			m_mulMaterialFactors[matIdx].m_specular = m_initMaterials[matIdx].m_specular;
			m_mulMaterialFactors[matIdx].m_specularPower = m_initMaterials[matIdx].m_specularPower;
			m_mulMaterialFactors[matIdx].m_ambient = m_initMaterials[matIdx].m_ambient;

			m_addMaterialFactors[matIdx] = initAdd;
		}
	}

	void MMDModel::EndMorphMaterial()
	{
		size_t matCount = m_materials.size();
		for (size_t matIdx = 0; matIdx < matCount; matIdx++)
		{
			MaterialFactor matFactor = m_mulMaterialFactors[matIdx];
			matFactor.Add(m_addMaterialFactors[matIdx], 1.0f);

			m_materials[matIdx].m_diffuse = matFactor.m_diffuse;
			m_materials[matIdx].m_alpha = matFactor.m_alpha;
			m_materials[matIdx].m_specular = matFactor.m_specular;
			m_materials[matIdx].m_specularPower = matFactor.m_specularPower;
			m_materials[matIdx].m_ambient = matFactor.m_ambient;
			m_materials[matIdx].m_textureMulFactor = m_mulMaterialFactors[matIdx].m_textureFactor;
			m_materials[matIdx].m_textureAddFactor = m_addMaterialFactors[matIdx].m_textureFactor;
			m_materials[matIdx].m_spTextureMulFactor = m_mulMaterialFactors[matIdx].m_spTextureFactor;
			m_materials[matIdx].m_spTextureAddFactor = m_addMaterialFactors[matIdx].m_spTextureFactor;
			m_materials[matIdx].m_toonTextureMulFactor = m_mulMaterialFactors[matIdx].m_toonTextureFactor;
			m_materials[matIdx].m_toonTextureAddFactor = m_addMaterialFactors[matIdx].m_toonTextureFactor;
		}
	}

	void MMDModel::MorphMaterial(const MaterialMorphData& morphData, float weight)
	{
		for (const auto& matMorph : morphData.m_materialMorphs)
		{
			if (matMorph.m_materialIndex != -1)
			{
				auto mi = matMorph.m_materialIndex;
				auto& mat = m_materials[mi];
				switch (matMorph.m_opType)
				{
				case saba::PMXMorph::MaterialMorph::OpType::Mul:
					m_mulMaterialFactors[mi].Mul(
						MaterialFactor(matMorph),
						weight
					);
					break;
				case saba::PMXMorph::MaterialMorph::OpType::Add:
					m_addMaterialFactors[mi].Add(
						MaterialFactor(matMorph),
						weight
					);
					break;
				default:
					break;
				}
			}
			else
			{
				switch (matMorph.m_opType)
				{
				case saba::PMXMorph::MaterialMorph::OpType::Mul:
					for (size_t i = 0; i < m_materials.size(); i++)
					{
						m_mulMaterialFactors[i].Mul(
							MaterialFactor(matMorph),
							weight
						);
					}
					break;
				case saba::PMXMorph::MaterialMorph::OpType::Add:
					for (size_t i = 0; i < m_materials.size(); i++)
					{
						m_addMaterialFactors[i].Add(
							MaterialFactor(matMorph),
							weight
						);
					}
					break;
				default:
					break;
				}
			}
		}
	}

	void MMDModel::MorphBone(const BoneMorphData& morphData, float weight)
	{
		for (auto& boneMorph : morphData.m_boneMorphs)
		{
			auto node = boneMorph.m_node;
			glm::vec3 t = glm::mix(glm::vec3(0), boneMorph.m_position, weight);
			node->m_translate = node->m_translate + t;
			glm::quat q = glm::slerp(node->m_rotate, boneMorph.m_rotate, weight);
			node->m_rotate = q;
		}
	}

	MMDModel::MaterialFactor::MaterialFactor(const saba::PMXMorph::MaterialMorph& pmxMat)
	{
		m_diffuse.r = pmxMat.m_diffuse.r;
		m_diffuse.g = pmxMat.m_diffuse.g;
		m_diffuse.b = pmxMat.m_diffuse.b;
		m_alpha = pmxMat.m_diffuse.a;
		m_specular = pmxMat.m_specular;
		m_specularPower = pmxMat.m_specularPower;
		m_ambient = pmxMat.m_ambient;
		m_edgeColor = pmxMat.m_edgeColor;
		m_edgeSize = pmxMat.m_edgeSize;
		m_textureFactor = pmxMat.m_textureFactor;
		m_spTextureFactor = pmxMat.m_sphereTextureFactor;
		m_toonTextureFactor = pmxMat.m_toonTextureFactor;
	}

	void MMDModel::MaterialFactor::Mul(const MaterialFactor& val, float weight)
	{
		m_diffuse = glm::mix(m_diffuse, m_diffuse * val.m_diffuse, weight);
		m_alpha = glm::mix(m_alpha, m_alpha * val.m_alpha, weight);
		m_specular = glm::mix(m_specular, m_specular * val.m_specular, weight);
		m_specularPower = glm::mix(m_specularPower, m_specularPower * val.m_specularPower, weight);
		m_ambient = glm::mix(m_ambient, m_ambient * val.m_ambient, weight);
		m_edgeColor = glm::mix(m_edgeColor, m_edgeColor * val.m_edgeColor, weight);
		m_edgeSize = glm::mix(m_edgeSize, m_edgeSize * val.m_edgeSize, weight);
		m_textureFactor = glm::mix(m_textureFactor, m_textureFactor * val.m_textureFactor, weight);
		m_spTextureFactor = glm::mix(m_spTextureFactor, m_spTextureFactor * val.m_spTextureFactor, weight);
		m_toonTextureFactor = glm::mix(m_toonTextureFactor, m_toonTextureFactor * val.m_toonTextureFactor, weight);
	}

	void MMDModel::MaterialFactor::Add(const MaterialFactor& val, float weight)
	{
		m_diffuse += val.m_diffuse * weight;
		m_alpha += val.m_alpha * weight;
		m_specular += val.m_specular * weight;
		m_specularPower += val.m_specularPower * weight;
		m_ambient += val.m_ambient * weight;
		m_edgeColor += val.m_edgeColor * weight;
		m_edgeSize += val.m_edgeSize * weight;
		m_textureFactor += val.m_textureFactor * weight;
		m_spTextureFactor += val.m_spTextureFactor * weight;
		m_toonTextureFactor += val.m_toonTextureFactor * weight;
	}
}
