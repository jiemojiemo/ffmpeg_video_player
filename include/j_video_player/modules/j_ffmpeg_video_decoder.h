//
// Created by user on 11/7/23.
//

#ifndef FFMPEG_VIDEO_PLAYER_J_FFMPEG_VIDEO_DECODER_H
#define FFMPEG_VIDEO_PLAYER_J_FFMPEG_VIDEO_DECODER_H
#include "j_video_player/ffmpeg_utils/ffmpeg_codec.h"
#include "j_video_player/ffmpeg_utils/ffmpeg_demuxer.h"
#include "j_video_player/ffmpeg_utils/ffmpeg_headers.h"
#include "j_video_player/modules/j_i_video_decoder.h"
#include "j_video_player/utils/scope_guard.h"
namespace j_video_player {
class FFmpegVideoDecoder : public IVideoDecoder {
public:
  ~FFmpegVideoDecoder() override { cleanUp(); }
  int open(const std::string &file_path) override {
    int ret = initDemuxer(file_path);
    RETURN_IF_ERROR(ret);

    ret = initCodec();
    RETURN_IF_ERROR(ret);

    return ret;
  }

  bool isValid() override {
    if (demux_ == nullptr || codec_ == nullptr) {
      return false;
    }

    return demux_->isValid() && codec_->isValid();
  }
  void close() override { cleanUp(); }
  std::shared_ptr<VideoFrame> decodeNextFrame() override {
    if (!isValid()) {
      LOGE("decoder is invalid");
      return nullptr;
    }

    for (;;) {
      int ret = 0;

      // there may remains frames, let's try to get frame from codec first,
      {
        auto video_frame = std::make_shared<VideoFrame>();
        ret = codec_->receiveFrame(video_frame->f);
        if (ret == 0) {
          updatePosition(video_frame);
          return video_frame;
        }
      }

      AVPacket *pkt = nullptr;
      std::tie(ret, pkt) = demux_->readPacket();
      ON_SCOPE_EXIT([&pkt] { av_packet_unref(pkt); });
      if (pkt == nullptr) {
        return nullptr;
      }

      // skip this packet if is not target stream
      if (stream_index_ != pkt->stream_index) {
        continue;
      }

      if (ret != 0) {
        LOGE("readPacket failed");
        return nullptr;
      }

      ret = codec_->sendPacketToCodec(pkt);
      if (ret == AVERROR_EOF) {
        LOGE("sendPacketToCodec AVERROR_EOF\n");
        return nullptr;
      }

      auto video_frame = std::make_shared<VideoFrame>();
      ret = codec_->receiveFrame(video_frame->f);
      if (ret == 0) {
        updatePosition(video_frame);
        return video_frame;
      } else if (ret == AVERROR(EAGAIN)) {
        continue;
      } else {
        LOGE("codec_->receiveFrame failed");
        return nullptr;
      }
    }
  }

  std::shared_ptr<VideoFrame> seekVideoFrameQuick(int64_t timestamp) override {
    if (!isValid()) {
      LOGE("seek failed");
      return nullptr;
    }

    auto ret = seekDemuxerAndFlushCodecBuffer(timestamp, position_);
    if (ret != 0) {
      return nullptr;
    }

    return decodeNextFrame();
  }

  std::shared_ptr<VideoFrame>
  seekVideoFramePrecise(int64_t timestamp) override {
    if (!isValid()) {
      LOGE("seek failed");
      return nullptr;
    }

    auto ret = seekDemuxerAndFlushCodecBuffer(timestamp, position_);
    if (ret != 0) {
      return nullptr;
    }

    for (;;) {
      auto frame = decodeNextFrame();
      if (frame == nullptr) {
        return nullptr;
      }

      auto frame_pts =
          av_rescale_q(frame->f->pts, getVideoStreamTimeBase(), AV_TIME_BASE_Q);
      if (frame_pts >= timestamp) {
        return frame;
      }
    }
  }

  int64_t getPosition() override { return position_; }

  AVRational getVideoStreamTimeBase() {
    return demux_->getStream(stream_index_)->time_base;
  }

  VideoFileInfo getVideoFileInfo() override {
    VideoFileInfo info;
    info.file_path = demux_->getFormatContext()->url;
    info.width = codec_->getCodecContext()->width;
    info.height = codec_->getCodecContext()->height;
    info.duration = demux_->getFormatContext()->duration;
    info.bit_rate = demux_->getFormatContext()->bit_rate;

    auto *video_stream = demux_->getStream(stream_index_);
    info.fps = av_q2d(video_stream->avg_frame_rate);
    info.video_stream_timebase = video_stream->time_base;
    return info;
  }

private:
  int initDemuxer(const std::string &file_path) {
    demux_ = std::make_unique<ffmpeg_utils::FFmpegDmuxer>();
    int ret = demux_->openFile(file_path);
    RETURN_IF_ERROR_LOG(ret, "demux_->openFile failed\n");
    return ret;
  }

  int initCodec() {
    codec_ = std::make_unique<ffmpeg_utils::FFmpegCodec>();
    stream_index_ = demux_->getVideoStreamIndex();
    auto *av_stream = demux_->getStream(stream_index_);
    if (av_stream == nullptr) {
      printf("av_stream is nullptr\n");
      return -1;
    }

    int ret =
        codec_->prepare(av_stream->codecpar->codec_id, av_stream->codecpar);
    RETURN_IF_ERROR_LOG(ret, "codec_->prepare failed\n");

    return ret;
  }

  void cleanUp() {
    demux_ = nullptr;
    codec_ = nullptr;
    position_ = AV_NOPTS_VALUE;
  }

  void updatePosition(const std::shared_ptr<VideoFrame> &frame) {
    if (frame->f->pts != AV_NOPTS_VALUE) {
      position_ = av_rescale_q(frame->f->pts,
                               demux_->getStream(stream_index_)->time_base,
                               AV_TIME_BASE_Q);
    }
  }

  int seekToTargetPosition(int64_t seek_pos, int64_t seek_rel) {
    auto seek_flags = 0;
    if (seek_rel < 0) {
      seek_flags = AVSEEK_FLAG_BACKWARD;
    }
    auto min_ts = (seek_rel > 0) ? (seek_pos - seek_rel + 2) : (INT64_MIN);
    auto max_ts = (seek_rel < 0) ? (seek_pos - seek_rel - 2) : (seek_pos);
    return demux_->seek(min_ts, seek_pos, max_ts, seek_flags);
  }

  int seekDemuxerAndFlushCodecBuffer(int64_t timestamp, int64_t target_pos) {
    auto ret = seekToTargetPosition(timestamp, timestamp - target_pos);
    if (ret != 0) {
      LOGE("seekToTargetPosition failed");
      return -1;
    }

    codec_->flush_buffers();
    return 0;
  }

  std::unique_ptr<ffmpeg_utils::FFmpegDmuxer> demux_{nullptr};
  std::unique_ptr<ffmpeg_utils::FFmpegCodec> codec_{nullptr};
  int stream_index_{-1};
  int64_t position_{AV_NOPTS_VALUE};
};

} // namespace j_video_player

#endif // FFMPEG_VIDEO_PLAYER_J_FFMPEG_VIDEO_DECODER_H
