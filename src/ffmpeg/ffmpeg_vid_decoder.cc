#include "ffmpeg_vid_decoder.h"
#include "buffer.h"
namespace easymedia {
FFMpegDecoder::FFMpegDecoder(const char *param)
    : pkt(nullptr), codec(nullptr), ffmpeg_context(nullptr) {
  std::map<std::string, std::string> params;
  std::list<std::pair<const std::string, std::string &>> req_list;

  std::string input_data_type;
  std::string output_data_type;
  std::string split_mode;
  req_list.push_back(std::pair<const std::string, std::string &>(
      KEY_INPUTDATATYPE, input_data_type));
  req_list.push_back(std::pair<const std::string, std::string &>(
      KEY_OUTPUTDATATYPE, output_data_type));
  req_list.push_back(std::pair<const std::string, std::string &>(
      KEY_MPP_SPLIT_MODE, split_mode));

  int ret = parse_media_param_match(param, params, req_list);
  if (ret == 0 || input_data_type.empty()) {
    RKMEDIA_LOGI("missing %s\n", KEY_INPUTDATATYPE);
    return;
  }
  if (!split_mode.empty())
    need_split = std::stoi(split_mode);
  if (!input_data_type.empty()) {
    codec_id = CodecTypeToAVCodecID(StringToCodecType(input_data_type.c_str()));
  }
  RKMEDIA_LOGI("codec_id = %d.\n", codec_id);
}

bool FFMpegDecoder::Init() {
  pkt = av_packet_alloc();
  if (!pkt) {
    return false;
  }
  codec = avcodec_find_decoder(codec_id);
  if (!codec) {
    RKMEDIA_LOGI("Codec not found,\n");
    return false;
  }

  if (need_split) {
    parser = av_parser_init(codec->id);
    if (!parser) {
      RKMEDIA_LOGI("parser not found\n");
      return false;
    }
  }

  ffmpeg_context = avcodec_alloc_context3(codec);
  if (!ffmpeg_context) {
    RKMEDIA_LOGI("Could not allocate video codec context.\n");
    return false;
  }

  /* For some codecs, such as msmpeg4 and mpeg4, width and height
    MUST be initialized there because this information is not
  available in the bitstream. */

  /* open it */
  if (avcodec_open2(ffmpeg_context, codec, NULL) < 0) {
    RKMEDIA_LOGI("Could not open codec\n");
    return false;
  }
  return true;
}

int FFMpegDecoder::Process(const std::shared_ptr<MediaBuffer> &input _UNUSED,
                           std::shared_ptr<MediaBuffer> &output _UNUSED,
                           std::shared_ptr<MediaBuffer> extra_output _UNUSED) {
  return 0;
}

int FFMpegDecoder::SendInput(const std::shared_ptr<MediaBuffer> &input) {
  // if(!support_async){
  //    errno = ENOSYS;
  //    return -ENOSYS;
  //}
  if (!input) {
    return 0;
  }
  int ret = 0;
  int data_size = 0;
  int one_frame_flag = 0;
  uint8_t *data = (uint8_t *)input->GetPtr();
  data_size = input->GetValidSize();
  if (need_split) {
    while (data_size > 0) {
      ret =
          av_parser_parse2(parser, ffmpeg_context, &pkt->data, &pkt->size, data,
                           data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
      if (ret < 0) {
        fprintf(stderr, "Error while parsing\n");
        return -1;
      }
      data += ret;
      data_size -= ret;
      input->SetValidSize(data_size);
      input->SetPtr(data);
      if (pkt->size) {
        ret = avcodec_send_packet(ffmpeg_context, pkt);
        if (ret < 0) {
          RKMEDIA_LOGI("%d: Error sending a packet for decoding\n", __LINE__);
          return ret;
        }
        if (ret == 0)
          one_frame_flag = 1;
      }
    }
  } else {
    pkt->data = data;
    pkt->size = data_size;
    ret = avcodec_send_packet(ffmpeg_context, pkt);
    if (ret < 0) {
      RKMEDIA_LOGI("Error sending a packet for decoding\n");
      return ret;
    }
    one_frame_flag = 1;
  }
  if (one_frame_flag)
    return 0;
  else
    return ret;
}

std::shared_ptr<MediaBuffer> FFMpegDecoder::FetchOutput() {
  int ret, size;
  auto frame = av_frame_alloc();
  if (!frame) {
    RKMEDIA_LOGI("create frame failed .\n");
    return nullptr;
  }
  ret = avcodec_receive_frame(ffmpeg_context, frame);
  if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
    RKMEDIA_LOGI("Error during decoding %d\n", ret);
    return nullptr;
  } else if (ret < 0) {
    RKMEDIA_LOGI("Error during decoding\n");
    return nullptr;
  }

  size = av_image_get_buffer_size((enum AVPixelFormat)frame->format,
                                  frame->width, frame->height, 1);
  auto &&buffer =
      MediaBuffer::Alloc2(size, MediaBuffer::MemType::MEM_HARD_WARE, 0);
  ImageInfo image_info;
  image_info.width = frame->width;
  image_info.height = frame->height;
  image_info.vir_width = frame->width;
  image_info.vir_height = frame->height;
  image_info.pix_fmt = AVPixFmtToPixFmt((enum AVPixelFormat)frame->format);
  auto buffer_out =
      std::make_shared<easymedia::ImageBuffer>(buffer, image_info);
  av_image_copy_to_buffer(
      (uint8_t *)buffer_out->GetPtr(), size,
      (const uint8_t *const *)frame->data, (const int *)frame->linesize,
      (enum AVPixelFormat)frame->format, frame->width, frame->height, 1);
  buffer_out->SetValidSize(size);
  buffer_out->SetUSTimeStamp(frame->pts);
  buffer_out->SetType(Type::Image);
  av_frame_free(&frame);
  return buffer_out;
}

FFMpegDecoder::~FFMpegDecoder() {
  if (parser)
    av_parser_close(parser);
  avcodec_free_context(&ffmpeg_context);
  av_packet_free(&pkt);
}
DEFINE_VIDEO_DECODER_FACTORY(FFMpegDecoder)
const char *FACTORY(FFMpegDecoder)::ExpectedInputDataType() {
  return TYPENEAR(IMAGE_JPEG) TYPENEAR(VIDEO_H264) TYPENEAR(VIDEO_H265);
}
const char *FACTORY(FFMpegDecoder)::OutPutDataType() { return IMAGE_NV12; }

} // namespace easymedia
