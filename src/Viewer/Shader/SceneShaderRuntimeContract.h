#pragma once

#include "Viewer/Shader/ShaderProgramDefinition.h"

namespace Chrivent {
	// 내장 모델 렌더링이 요구하는 표면·외곽선·지면 그림자 패스를 보관한다.
	struct BuiltInShaderPasses {
		ShaderProgramDefinition model;
		ShaderProgramDefinition edge;
		ShaderProgramDefinition groundShadow;
	};

	// API 구현에서 파일명과 진입점을 알지 않도록 엔진 장면 입력 패스를 역할별로 보관한다.
	struct SceneInputShaderPasses {
		ShaderProgramDefinition depth;
		ShaderProgramDefinition velocity;
	};

	// 엔진 장면 렌더링에 필요한 내장 패스와 보조 입력 패스를 하나의 주입 계약으로 묶는다.
	struct SceneShaderRuntimeContract {
		BuiltInShaderPasses builtIn;
		SceneInputShaderPasses sceneInput;
	};
}
