#include "ModelMorph.h"

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
		out.cartoonTextureFactor = glm::mix(out.cartoonTextureFactor, out.cartoonTextureFactor * val.cartoonTextureFactor, weight);
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
		out.cartoonTextureFactor += val.cartoonTextureFactor * weight;
	}

	void ModelMorph::EvalMorph(const Morph* morph, const float morphWeight) const {
		if (std::abs(morphWeight) <= std::numeric_limits<float>::epsilon())
			return;
		switch (morph->morphType) {
			case MorphType::Position:
				MorphPosition(model.positionMorphs[morph->dataIndex], morphWeight);
				break;
			case MorphType::Uv:
				MorphUv(model.uvMorphs[morph->dataIndex], morphWeight);
				break;
			case MorphType::Material:
				MorphMaterial(model.materialMorphs[morph->dataIndex], morphWeight);
				break;
			case MorphType::Bone:
				MorphBone(model.boneMorphs[morph->dataIndex], morphWeight);
				break;
			case MorphType::Group: {
				for (const auto& [morphIndex, weight] : model.groupMorphs[morph->dataIndex]) {
					if (morphIndex == -1)
						continue;
					EvalMorph(model.morphs[morphIndex].get(), weight * morphWeight);
				}
				break;
			}
			case MorphType::AddUv1:
				break;
			case MorphType::AddUv2:
				break;
			case MorphType::AddUv3:
				break;
			case MorphType::AddUv4:
				break;
			case MorphType::Flip:
				break;
			case MorphType::Impulse:
				break;
		}
	}

	void ModelMorph::MorphPosition(const std::vector<PositionMorph>& morphData, const float weight) const {
		for (const auto& [vertexIndex, position] : morphData)
			model.morphPositions[vertexIndex] += position * weight;
	}

	void ModelMorph::MorphUv(const std::vector<UvMorph>& morphData, const float weight) const {
		for (const auto& [vertexIndex, uv] : morphData)
			model.morphUVs[vertexIndex] += uv * weight;
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
		for (size_t i = 0; i < model.materials.size(); i++) {
			auto& mul = model.mulMaterialFactors[i];
			mul = initMul;
			mul.diffuse       = model.initMaterials[i].diffuse;
			mul.specular      = model.initMaterials[i].specular;
			mul.specularPower = model.initMaterials[i].specularPower;
			mul.ambient       = model.initMaterials[i].ambient;
			model.addMaterialFactors[i] = initAdd;
		}
	}

	void ModelMorph::EndMorphMaterial() const {
		for (size_t i = 0; i < model.materials.size(); i++) {
			auto& mat = model.materials[i];
			const auto& mul = model.mulMaterialFactors[i];
			const auto& add = model.addMaterialFactors[i];
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
			mat.cartoonTextureMulFactor = mul.cartoonTextureFactor;
			mat.cartoonTextureAddFactor = add.cartoonTextureFactor;
		}
	}

	void ModelMorph::MorphMaterial(const std::vector<MaterialMorph>& morphData, const float weight) const {
		for (const auto& matMorph : morphData) {
			auto Apply = [&](const size_t mi) {
				switch (matMorph.opType) {
					case OpType::Mul: AccumulateMaterialMul(model.mulMaterialFactors[mi], matMorph, weight); break;
					case OpType::Add: AccumulateMaterialAdd(model.addMaterialFactors[mi], matMorph, weight); break;
				}
			};
			if (matMorph.materialIndex != -1)
				Apply(static_cast<size_t>(matMorph.materialIndex));
			else {
				for (size_t i = 0; i < model.materials.size(); i++)
					Apply(i);
			}
		}
	}

	void ModelMorph::MorphBone(const std::vector<BoneMorph>& morphData, const float weight) const {
		for (const auto& [boneIndex, position, quaternion] : morphData) {
			auto* node = model.nodes[boneIndex].get();
			node->translate += position * weight;
			const glm::quat q = glm::slerp(glm::quat(1,0,0,0), quaternion, weight);
			node->rotate = glm::normalize(q * node->rotate);
		}
	}

	void ModelMorph::Update() const {
		BeginMorphMaterial();
		for (const auto& morph : model.morphs)
			EvalMorph(morph.get(), morph->weight);
		EndMorphMaterial();
	}
}
