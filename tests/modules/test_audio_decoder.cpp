//
// Created by user on 5/22/23.
//
#include "j_video_player/modules/j_audio_decoder.h"
#include "j_video_player/modules/j_audio_render.h"
#include <gmock/gmock.h>

using namespace testing;
using namespace j_video_player;

class MockAudioRender : public IAudioRenderDeprecated {
public:
  MOCK_METHOD(void, initAudioRender, (), (override));
  void uninit() override {}
  void clearAudioCache() override {}
  void setAudioCallback(std::function<void(uint8_t *, int)> func) override {
    (void)(func);
  }
  int getAudioCacheRemainSize() override { return 0; }
};

class AAudioDecoder : public Test {
public:
  std::string filepath =
      "/Users/user/Documents/work/测试视频/video_1280x720_30fps_30sec.mp4";
  AudioDecoder d{filepath};
  std::shared_ptr<MockAudioRender> render = std::make_shared<MockAudioRender>();
};

TEST_F(AAudioDecoder, CanCreateAudioDecoderWithFilePath) {
  auto dd = AudioDecoder(filepath);

  ASSERT_THAT(dd.getURL(), Eq(filepath));
}

TEST_F(AAudioDecoder, DecodeTypeIsAudio) {
  ASSERT_THAT(d.getMediaType(), Eq(AVMEDIA_TYPE_AUDIO));
}

TEST_F(AAudioDecoder, PrepareDecoderWithAudioResampler) {
  d.onPrepareDecoder();

  ASSERT_THAT(d.getResampler(), NotNull());
}

TEST_F(AAudioDecoder, DecodecDoneWillReleaseAudioResampler) {
  d.onPrepareDecoder();
  d.OnDecoderDone();

  ASSERT_THAT(d.getResampler(), IsNull());
}

TEST_F(AAudioDecoder, PrepareDecoderAlsoInitRender) {
  d.setRender(render);
  EXPECT_CALL(*render, initAudioRender()).Times(1);
  d.onPrepareDecoder();
}
