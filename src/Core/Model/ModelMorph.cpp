#include "Core/Model/ModelMorph.h"

#include <limits>

namespace Chrivent {
	void ModelMorph::AccumulateMaterialMul(MaterialMorph& out, const MaterialMorph& val, const float weight) {
		out.diffuse = glm::mix(out.diffuse, out.diffuse * val.diffuse, weight);
		out.specular = glm::mix(out.specular, out.specular * val.specular, weight);
		out.specularPower = glm::mix(out.specularPower, out.specularPower * val.specularPower, weight);
		out.ambient = glm::mix(out.ambient, out.ambient * val.ambient, weight);
		out.edgeColor = glm::mix(out.edgeColor, out.edgeColor * val.edgeColor, weight);
		out.edgeSize = glm::mix(out.edgeSize, out.edgeSize * val.edgeSize, weight);
		out.textureFactor = glm::mix(out.textureFactor, out.textureFactor * val.textureFactor, weight);
		out.sphereTextureFactor = glm::mix(out.sphereTextureFactor, out.sphereTextureFactor * val.sphereTextureFactor, weight);
		out.toonTextureFactor = glm::mix(out.toonTextureFactor, out.toonTextureFactor * val.toonTextureFactor, weight);
	}

	void ModelMorph::AccumulateMaterialAdd(MaterialMorph& out, const MaterialMorph& val, const float weight) {
		out.diffuse += val.diffuse * weight;
		out.specular += val.specular * weight;
		out.specularPower += val.specularPower * weight;
		out.ambient += val.ambient * weight;
		out.edgeColor += val.edgeColor * weight;
		out.edgeSize += val.edgeSize * weight;
		out.textureFactor += val.textureFactor * weight;
		out.sphereTextureFactor += val.sphereTextureFactor * weight;
		out.toonTextureFactor += val.toonTextureFactor * weight;
	}

	void ModelMorph::EvalMorph(const Morph* morph, const float morphWeight) {
		pendingMorphs.emplace_back(morph, morphWeight);
		while (!pendingMorphs.empty()) {
			const auto [currentMorph, currentWeight] = pendingMorphs.back();
			pendingMorphs.pop_back();
			if (std::abs(currentWeight) <= std::numeric_limits<float>::epsilon())
				continue;
			const MorphType type = currentMorph->morphType;
			if (type == MorphType::Position)
				MorphPosition(model.morphData.positionMorphs[currentMorph->dataIndex], currentWeight);
			else if (type == MorphType::Uv)
				MorphUv(model.morphData.uvMorphs[currentMorph->dataIndex], currentWeight);
			else if (type == MorphType::Material)
				MorphMaterial(model.morphData.materialMorphs[currentMorph->dataIndex], currentWeight);
			else if (type == MorphType::Bone)
				MorphBone(model.morphData.boneMorphs[currentMorph->dataIndex], currentWeight);
			else if (type == MorphType::Group) {
				const auto& children = model.morphData.groupMorphs[currentMorph->dataIndex];
				for (std::size_t index = children.size(); index > 0; index--) {
					const auto& [morphIndex, weight] = children[index - 1];
					if (morphIndex != -1) {
						pendingMorphs.emplace_back(
							model.morphData.GetMorphs()[morphIndex].get(), weight * currentWeight);
					}
				}
			}
		}
	}

	void ModelMorph::MorphPosition(const std::vector<PositionMorph>& morphData, const float weight) const {
		for (const auto& [vertexIndex, position] : morphData)
			model.morphData.morphPositions[vertexIndex] += position * weight;
	}

	void ModelMorph::MorphUv(const std::vector<UvMorph>& morphData, const float weight) const {
		for (const auto& [vertexIndex, uv] : morphData)
			model.morphData.morphUVs[vertexIndex] += uv * weight;
	}

	void ModelMorph::BeginMorphMaterial() const {
		constexpr MaterialMorph initMul{
			0, OpType::Mul, glm::vec4(1), glm::vec3(1), 1.0f, glm::vec3(1),
			glm::vec4(1), 1.0f, glm::vec4(1), glm::vec4(1), glm::vec4(1)
		};
		constexpr MaterialMorph initAdd{
			0, OpType::Add, glm::vec4(0), glm::vec3(0), 0.0f, glm::vec3(0),
			glm::vec4(0), 0.0f, glm::vec4(0), glm::vec4(0), glm::vec4(0)
		};
		for (size_t i = 0; i < model.materialData.materials.size(); i++) {
			auto& mul = model.materialData.mulMaterialFactors[i];
			mul = initMul;
			mul.diffuse       = model.materialData.initMaterials[i].diffuse;
			mul.specular      = model.materialData.initMaterials[i].specular;
			mul.specularPower = model.materialData.initMaterials[i].specularPower;
			mul.ambient       = model.materialData.initMaterials[i].ambient;
			model.materialData.addMaterialFactors[i] = initAdd;
		}
	}

	void ModelMorph::EndMorphMaterial() const {
		for (size_t i = 0; i < model.materialData.materials.size(); i++) {
			auto& mat = model.materialData.materials[i];
			const auto& mul = model.materialData.mulMaterialFactors[i];
			const auto& add = model.materialData.addMaterialFactors[i];
			auto matFactor = mul;
			AccumulateMaterialAdd(matFactor, add, 1.0f);
			mat.diffuse        = matFactor.diffuse;
			mat.specular       = matFactor.specular;
			mat.specularPower  = matFactor.specularPower;
			mat.ambient        = matFactor.ambient;
			mat.textureMulFactor   = mul.textureFactor;
			mat.textureAddFactor   = add.textureFactor;
			mat.sphereTextureMulFactor = mul.sphereTextureFactor;
			mat.sphereTextureAddFactor = add.sphereTextureFactor;
			mat.toonTextureMulFactor = mul.toonTextureFactor;
			mat.toonTextureAddFactor = add.toonTextureFactor;
		}
	}

	void ModelMorph::MorphMaterial(const std::vector<MaterialMorph>& morphData, const float weight) const {
		for (const auto& matMorph : morphData) {
			auto Apply = [&](const size_t mi) {
				switch (matMorph.opType) {
					case OpType::Mul: AccumulateMaterialMul(model.materialData.mulMaterialFactors[mi], matMorph, weight); break;
					case OpType::Add: AccumulateMaterialAdd(model.materialData.addMaterialFactors[mi], matMorph, weight); break;
				}
			};
			if (matMorph.materialIndex != -1)
				Apply(matMorph.materialIndex);
			else {
				for (size_t i = 0; i < model.materialData.materials.size(); i++)
					Apply(i);
			}
		}
	}

	void ModelMorph::MorphBone(const std::vector<BoneMorph>& morphData, const float weight) const {
		for (const auto& [boneIndex, position, quaternion] : morphData) {
			auto* node = model.skeletonData.GetNodes()[boneIndex].get();
			node->translate += position * weight;
			const glm::quat q = glm::slerp(glm::quat(1,0,0,0), quaternion, weight);
			node->rotate = glm::normalize(q * node->rotate);
		}
	}

	void ModelMorph::Update() {
		BeginMorphMaterial();
		pendingMorphs.clear();
		const auto& morphs = model.morphData.GetMorphs();
		if (pendingMorphs.capacity() < morphs.size())
			pendingMorphs.reserve(morphs.size());
		for (const auto& morph : morphs) {
			if (std::abs(morph->weight) > std::numeric_limits<float>::epsilon())
				EvalMorph(morph.get(), morph->weight);
		}
		EndMorphMaterial();
	}
}
