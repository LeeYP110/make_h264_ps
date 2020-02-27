#pragma once
#include "PsHeader.h"

class MakePsPacket {
  public:
    typedef std::shared_ptr<MakePsPacket> Ptr;

    MakePsPacket();
    virtual ~MakePsPacket();

    uint32_t MakeRtpStream(unsigned char* ori_data,
                           uint64_t data_len,
                           FrameInfo::Ptr frame_info,
                           unsigned char* dst,
                           bool is_need_ps);
    uint32_t MakePsStream(unsigned char* ori_data,
                          uint64_t data_len,
                          FrameInfo::Ptr frame_info,
                          unsigned char* dst);
  private:
    int MakeRtpHeader(unsigned char * header_data, int marker_flag, uint16_t cseq, uint32_t pts, uint32_t ssrc);

    int MakePsHeader(unsigned char* header_data, uint64_t scr);
    int MakeSysHeader(unsigned char* header_data);
    int MakePsmHeader(unsigned char* header_data);
    int MakePesHeader(unsigned char* header_data, int stream_id, int payload_len, uint64_t pts, uint64_t dts);

    int SetPesPayloadLen(unsigned char* header_data, int payload_len);
    int SetPesPtsDts(unsigned char* header_data, uint64_t pts, uint64_t dts);
  private:

};

