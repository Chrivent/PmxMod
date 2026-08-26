#include "Program/Config.h"

#include <gtest/gtest.h>

#include <chrono>
#include <fstream>
#include <string>

namespace Chrivent {
	// 씬 설정 파일 파싱이 잘못된 입력에서 기존 설정을 보존하는지 검증한다.
	class SceneConfigContractTest : public testing::Test {
	protected:
		std::filesystem::path scenePath = std::filesystem::temp_directory_path()
			/ ("PmxModSceneConfigTest_" + std::to_string(
				std::chrono::steady_clock::now().time_since_epoch().count()) + ".pmscene");

		// 테스트용 씬 파일 내용을 바이너리 모드로 기록한다.
		void WriteScene(const std::string& contents) const {
			std::ofstream stream(scenePath, std::ios::binary);
			stream << contents;
			ASSERT_TRUE(stream.good());
		}

		// 테스트가 만든 임시 씬 파일을 제거한다.
		void TearDown() override {
			std::error_code error;
			std::filesystem::remove(scenePath, error);
		}
	};

	TEST_F(SceneConfigContractTest, LoadsValidSceneData) {
		WriteScene(
			"PmxModScene\n"
			"camera\tcamera.vmd\n"
			"music\tmusic.wav\n"
			"models\t1\n"
			"model\t1.5\t1\tmodel.pmx\n"
			"anim\tmotion.vmd\n");
		SceneConfig config;
		ASSERT_TRUE(config.Load(scenePath));
		ASSERT_EQ(config.modelConfigs.size(), 1);
		EXPECT_FLOAT_EQ(config.modelConfigs[0].scale, 1.5f);
		EXPECT_EQ(config.modelConfigs[0].animPaths.size(), 1);
		EXPECT_EQ(config.cameraAnim, std::filesystem::path("camera.vmd"));
		EXPECT_EQ(config.musicPath, std::filesystem::path("music.wav"));
	}

	TEST_F(SceneConfigContractTest, RejectsInvalidNumbersWithoutChangingCurrentScene) {
		WriteScene(
			"PmxModScene\n"
			"camera\t\n"
			"music\t\n"
			"models\tnot-a-number\n");
		SceneConfig config;
		config.cameraAnim = "current.vmd";
		EXPECT_FALSE(config.Load(scenePath));
		EXPECT_EQ(config.cameraAnim, std::filesystem::path("current.vmd"));
	}

	TEST_F(SceneConfigContractTest, RejectsNonFiniteScale) {
		WriteScene(
			"PmxModScene\n"
			"camera\t\n"
			"music\t\n"
			"models\t1\n"
			"model\tnan\t0\tmodel.pmx\n");
		SceneConfig config;
		EXPECT_FALSE(config.Load(scenePath));
	}

	TEST_F(SceneConfigContractTest, RejectsUnboundedEntryCounts) {
		WriteScene(
			"PmxModScene\n"
			"camera\t\n"
			"music\t\n"
			"models\t100001\n");
		SceneConfig config;
		EXPECT_FALSE(config.Load(scenePath));
	}

	TEST_F(SceneConfigContractTest, RejectsTruncatedSceneWithoutChangingCurrentScene) {
		WriteScene(
			"PmxModScene\n"
			"camera\tnew.vmd\n"
			"music\t\n"
			"models\t1\n");
		SceneConfig config;
		config.musicPath = "current.wav";
		EXPECT_FALSE(config.Load(scenePath));
		EXPECT_EQ(config.musicPath, std::filesystem::path("current.wav"));
	}
}
