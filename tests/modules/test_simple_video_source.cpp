//
// Created by user on 11/11/23.
//

#include "j_video_player/modules/j_simple_video_source.h"
#include <gmock/gmock.h>
using namespace testing;
using namespace j_video_player;

class ASimpleVideoSource : public Test {
public:
  void SetUp() override {
    source = std::make_unique<SimpleVideoSource>(decoder);
  }

  std::shared_ptr<FFmpegVideoDecoder> decoder =
      std::make_shared<FFmpegVideoDecoder>();
  std::unique_ptr<SimpleVideoSource> source = nullptr;
  std::string url =
      "/Users/user/Documents/work/测试视频/video_1280x720_30fps_30sec.mp4";
};

TEST_F(ASimpleVideoSource, CreateWithADecoder) {
  source = std::make_unique<SimpleVideoSource>(decoder);
}

TEST_F(ASimpleVideoSource, CanOpenAFile) {
  source = std::make_unique<SimpleVideoSource>(decoder);
  int ret = source->open(url);

  ASSERT_THAT(ret, Eq(0));
}

TEST_F(ASimpleVideoSource, OpenFailedIfURLIsInvalid) {
  source = std::make_unique<SimpleVideoSource>(decoder);
  int ret = source->open("file://test.mp4");

  ASSERT_THAT(ret, Not(0));
}

TEST_F(ASimpleVideoSource, OpenFailedIfDecoderIsNull) {
  source = std::make_unique<SimpleVideoSource>(nullptr);
  int ret = source->open(url);

  ASSERT_THAT(ret, Not(0));
}

TEST_F(ASimpleVideoSource, CanGetMediaFileInfoAfterOpen) {
  source->open(url);
  auto info = source->getMediaFileInfo();

  ASSERT_THAT(info.width, Not(0));
}

TEST_F(ASimpleVideoSource, InitStateIsIdle) {
  ASSERT_THAT(source->getState(), Eq(VideoSourceState::kIdle));
}

TEST_F(ASimpleVideoSource, CanPlayIfOpenSuccess) {
  source->open(url);
  int ret = source->play();

  ASSERT_THAT(ret, Eq(0));
}

TEST_F(ASimpleVideoSource, PlayChangeStateToPlaying) {
  source->open(url);
  source->play();

  ASSERT_THAT(source->getState(), Eq(VideoSourceState::kPlaying));
}

TEST_F(ASimpleVideoSource, StopChangeStateToStopped) {
  source->open(url);
  source->play();
  source->stop();

  ASSERT_THAT(source->getState(), Eq(VideoSourceState::kStopped));
}

TEST_F(ASimpleVideoSource, PauseChangeStateToPaused) {
  source->open(url);
  source->play();
  source->pause();

  ASSERT_THAT(source->getState(), Eq(VideoSourceState::kPaused));
}

TEST_F(ASimpleVideoSource, QueueSizeIsZeroIfNeverPlay) {
  source->open(url);

  ASSERT_THAT(source->getQueueSize(), Eq(0));
}

TEST_F(ASimpleVideoSource, QueueSizeIncreasedAfterPlay) {
  source->open(url);
  source->play();

  for (;;) {
    if (source->getQueueSize() > 0)
      break;
  }

  ASSERT_THAT(source->getQueueSize(), Gt(0));
}

TEST_F(ASimpleVideoSource, CanDequeAFrameAfterPlay) {
  source->open(url);
  source->play();

  for (;;) {
    if (source->getQueueSize() > 0)
      break;
  }

  auto f = source->dequeueFrame();

  ASSERT_THAT(f, NotNull());
}

TEST_F(ASimpleVideoSource, DequeANullIfThereNoFrame) {
  auto f = source->dequeueFrame();

  ASSERT_THAT(f, IsNull());
}

TEST_F(ASimpleVideoSource, QueueSizeIncreasedAfterSeek) {
  source->open(url);
  source->seek(1000 * 1000);

  for (;;) {
    if (source->getQueueSize() > 0)
      break;
  }

  ASSERT_THAT(source->getQueueSize(), Gt(0));
}

TEST_F(ASimpleVideoSource, CanDequeASeekedFrameAfterSeek) {
  source->open(url);
  source->seek(1000 * 1000);

  for (;;) {
    if (source->getQueueSize() > 0)
      break;
  }

  auto f = source->dequeueFrame();

  ASSERT_THAT(f, NotNull());
  ASSERT_THAT(f->pts(), Eq(1000 * 1000));
}