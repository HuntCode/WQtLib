extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <vector>

// 你的 Extradata (AirPlay AAC-ELD 配置)
const uint8_t g_extradata[] = { 0xF8, 0xE8, 0x50, 0x00 };

int main() {
    // 1. 打开 Dump 文件
    const char* input_filename = "airplay_dump.bin"; // 确保这个文件在运行目录下
    FILE* in_file = fopen(input_filename, "rb");
    if (!in_file) {
        printf("Error: Cannot open input file %s\n", input_filename);
        printf("Hint: Make sure to dump data with [4-byte-len][data] format.\n");
        return -1;
    }

    FILE* out_file = fopen("output.pcm", "wb");
    if (!out_file) {
        printf("Error: Cannot create output file.\n");
        fclose(in_file);
        return -1;
    }

    // 2. 初始化 FFmpeg 原生解码器
    const AVCodec* codec = avcodec_find_decoder(AV_CODEC_ID_AAC);
    AVCodecContext* c = avcodec_alloc_context3(codec);

    // 设置 Extradata
    c->extradata = (uint8_t*)av_malloc(sizeof(g_extradata) + AV_INPUT_BUFFER_PADDING_SIZE);
    memcpy(c->extradata, g_extradata, sizeof(g_extradata));
    memset(c->extradata + sizeof(g_extradata), 0, AV_INPUT_BUFFER_PADDING_SIZE);
    c->extradata_size = sizeof(g_extradata);

    // 设置声道 (FFmpeg 7.0 API)
    AVChannelLayout ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    av_channel_layout_copy(&c->ch_layout, &ch_layout);
    c->sample_rate = 44100;

    if (avcodec_open2(c, codec, NULL) < 0) {
        printf("Error: Could not open codec.\n");
        return -1;
    }
    printf("Decoder opened: %s\n", codec->long_name);

    // 3. 初始化重采样 (FLTP -> S16)
    SwrContext* swr_ctx = nullptr;
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    AVChannelLayout in_ch_layout = AV_CHANNEL_LAYOUT_STEREO; // AAC 默认输出一般与输入一致

    int ret = swr_alloc_set_opts2(&swr_ctx,
                                  &out_ch_layout, AV_SAMPLE_FMT_S16, 44100,
                                  &in_ch_layout,  AV_SAMPLE_FMT_FLTP, 44100,
                                  0, nullptr);
    swr_init(swr_ctx);

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    int packet_count = 0;
    int32_t payload_len = 0;

    // ==========================================
    // 4. 循环读取文件
    // ==========================================
    // 假设格式为：[4字节长度][数据Payload]
    while (fread(&payload_len, sizeof(int32_t), 1, in_file) == 1) {

        // 安全检查：防止脏数据导致分配过大内存
        if (payload_len <= 0 || payload_len > 1024 * 1024) {
            printf("Warning: Invalid packet length %d, stopping.\n", payload_len);
            break;
        }

        // 为 Packet 分配数据空间
        if (av_new_packet(pkt, payload_len) < 0) {
            printf("Error: OOM\n");
            break;
        }

        // 读取实际音频数据
        if (fread(pkt->data, 1, payload_len, in_file) != (size_t)payload_len) {
            printf("Error: Unexpected EOF reading payload.\n");
            av_packet_unref(pkt);
            break;
        }

        // 发送给解码器
        ret = avcodec_send_packet(c, pkt);
        if (ret < 0) {
            printf("Error sending packet #%d\n", packet_count);
        } else {
            // 接收解码后的帧
            while (ret >= 0) {
                ret = avcodec_receive_frame(c, frame);
                if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) break;
                else if (ret < 0) {
                    printf("Error decoding frame\n");
                    break;
                }

                // 重采样并写入文件
                int max_dst_nb_samples = av_rescale_rnd(frame->nb_samples, 44100, 44100, AV_ROUND_UP);
                uint8_t* dst_data = NULL;
                int dst_linesize;
                av_samples_alloc(&dst_data, &dst_linesize, 2, max_dst_nb_samples, AV_SAMPLE_FMT_S16, 0);

                int num_samples = swr_convert(swr_ctx, &dst_data, max_dst_nb_samples,
                                              (const uint8_t**)frame->data, frame->nb_samples);

                if (num_samples > 0) {
                    fwrite(dst_data, 1, num_samples * 2 * 2, out_file); // S16 * Stereo
                }

                if (dst_data) av_freep(&dst_data);
                av_frame_unref(frame);
            }
        }

        av_packet_unref(pkt); // 释放本次包的引用，准备下一次
        packet_count++;
        if (packet_count % 100 == 0) printf("Processed %d packets...\r", packet_count);
    }

    printf("\nDone! Processed total %d packets.\n", packet_count);

    // 清理资源
    fclose(in_file);
    fclose(out_file);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&c);
    swr_free(&swr_ctx);

    return 0;
}
