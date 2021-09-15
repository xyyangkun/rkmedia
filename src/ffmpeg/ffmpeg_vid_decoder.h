// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef __FFMPEG_VID_DECODER_
#define __FFMPEG_VID_DECODER_
#include "decoder.h"
#include "ffmpeg_utils.h"
namespace easymedia {
class FFMpegDecoder : public VideoDecoder {
public:
  FFMpegDecoder(const char *param);
  virtual ~FFMpegDecoder();
  static const char *GetCodecName() { return "ffmpeg_vid"; }

  virtual bool Init() override;
  virtual int Process(const std::shared_ptr<MediaBuffer> &input,
                      std::shared_ptr<MediaBuffer> &output,
                      std::shared_ptr<MediaBuffer> extra_output) override;
  virtual int SendInput(const std::shared_ptr<MediaBuffer> &input) override;
  virtual std::shared_ptr<MediaBuffer> FetchOutput() override;

private:
  int need_split;
  AVCodecID codec_id;
  bool support_sync;
  bool support_async;
  AVPacket *pkt;
  AVCodec *codec;
  AVCodecContext *ffmpeg_context;
  AVCodecParserContext *parser;
};
} // namespace easymedia
#endif // #ifndef __FFMPEG_VID_DECODER_
