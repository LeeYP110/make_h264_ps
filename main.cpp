#include "MakePsPacket.h"
#include <list>

#define BUFF_SIZE 1024

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/dict.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
}
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "swscale.lib")
#pragma comment(lib, "swresample.lib")

FILE* w_file;
MakePsPacket make_packet;
AVFormatContext* ic = nullptr;
int videoIndex = 0;
int audioIndex = 1;

static double r2d(AVRational r) {
    return r.den == 0 ? 0 : (double)r.num / (double)r.den;
}

bool ReadH264(const char* url) {
    // ��ʼ����װ��
    //avcodec_register_all();

    // ��ʼ������⣨rtsp��rtmp��http��
    avformat_network_init();

    // ���ò���
    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "rtsp_transport", "tcp", 0);
    av_dict_set(&opts, "max_delay", "500", 0);

    int result = avformat_open_input(
                     &ic,
                     url,
                     nullptr,// nullptr ��ʾ�Զ�ѡ����װ��
                     &opts
                 );

    if (result != 0) {
        char buf[1024] = { 0 };
        av_strerror(result, buf, sizeof(buf) - 1);
        std::cout << buf << std::endl;
        return false;
    }

    std::cout << "open " << url << " success" << std::endl;

    // retrieve stream information
    result = avformat_find_stream_info(ic, NULL);
    if (result < 0) {
        fprintf(stderr, "Could not find stream information\n");
    }

    // ��ʱ�� ����
    int totalMs = ic->duration / (AV_TIME_BASE / 1000);
    std::cout << "TotalMs: " << totalMs << std::endl;
    //av_dump_format(ic, 0, ic->url, 0);

    videoIndex = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    AVStream* as = ic->streams[videoIndex];
    std::cout << "///////////////////video//////////////////" << std::endl;
    std::cout << "format:" << as->codecpar->format << std::endl;
    std::cout << "codec_id:" << as->codecpar->codec_id << std::endl;
    std::cout << "width:" << as->codecpar->width << std::endl;// width��height�п��ܲ�����
    std::cout << "height:" << as->codecpar->height << std::endl;
    std::cout << "video fps:" << r2d(as->avg_frame_rate) << std::endl;// ֡�� fps ����ת��

    //width = as->codecpar->width;
    //height = as->codecpar->height;

    // һ֡���� ��ͨ����������
    //std::cout << "audio frame_size:" << as->codecpar->frame_size << std::endl;
    // as->codecpar->frame_size * as->codecpar->channels * ������ʽռ���ֽ�
}

AVPacket* GetPacket() {
    if (ic == nullptr) {
        return nullptr;
    }

    AVPacket* pkt = av_packet_alloc();

    // ��ȡһ֡������ռ�
    int re = av_read_frame(ic, pkt);
    if (re != 0) {
        av_packet_free(&pkt);
        return nullptr;
    }

    // ptsת��Ϊms
    //pkt->pts = pkt->pts * (r2d(ic->streams[pkt->stream_index]->time_base) * 1000);
    //pkt->dts = pkt->dts * (r2d(ic->streams[pkt->stream_index]->time_base) * 1000);
    //std::cout << "pkt->pts" <<  pkt->pts << std::flush << std::endl;
    return pkt;
}

void FreePacket(AVPacket* pkt) {
    av_packet_free(&pkt);
    pkt = nullptr;
}

int pts = 0;
int count = 0;
void H264ToPs(AVPacket* pkt) {
    int64_t buf_size = pkt->size + 512;
    //std::cout << "buf_size: " << buf_size << " scr: " << scr/3600 << std::endl;

    unsigned char* ps_buf = new unsigned char[buf_size];
    memset(ps_buf, 0, buf_size);

    FrameInfo::Ptr frame_info = std::make_shared<FrameInfo>();

    static bool i_frame = false;
    if (pkt->flags) {
        frame_info->is_i_frame = true;
        //i_frame = true;
    }

    if (i_frame == false) {
        //return;
    }

    ////if (pkt->pts < 0 || pkt->dts < 0)
    //{
    //    pkt->pts = pts;
    //    pkt->dts = pts;
    //}
    frame_info->pts = pts;
    frame_info->dts = pts;
    frame_info->scr = pts;

    pts += 3600;

    auto char_size = sizeof(unsigned char);
    int len = make_packet.MakePsStream(pkt->data, pkt->size, frame_info, ps_buf);
    auto size = fwrite(ps_buf, char_size, len, w_file);
    fflush(w_file);
    if (size != len) {
        std::cout << "size:len" << size << ":" << len << std::endl;
    }

    //count += len;
    delete ps_buf;
    ps_buf = nullptr;
}

std::string url = "Onion.264";
int main() {
    url = "http://ivi.bupt.edu.cn/hls/cctv1hd.m3u8";
    //url = "rtsp://admin:admin123@10.169.241.24/cam/realmonitor?channel=1&subtype=0";
    //url = "test.mp4";
    ReadH264(url.c_str());

    w_file = fopen("h264.ps", "wb");

    int i = 0;
    while (i < 25 * 60) {
        AVPacket* pkt = GetPacket();
        if (pkt == nullptr) {
            break;
        }

        if (pkt->stream_index == videoIndex) {
            H264ToPs(pkt);
            ++i;
        }
        FreePacket(pkt);
    }
    fclose(w_file);
    return 0;
}