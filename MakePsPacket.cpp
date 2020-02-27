#include "MakePsPacket.h"



MakePsPacket::MakePsPacket() {
}


MakePsPacket::~MakePsPacket() {
}

uint32_t MakePsPacket::MakeRtpStream(unsigned char * ori_data,
                                     uint64_t data_len,
                                     FrameInfo::Ptr frame_info,
                                     unsigned char* dst,
                                     bool is_need_ps) {
    uint32_t stream_len = data_len;
    unsigned char* head_data = dst + 12; // 为rtp包头留出空间

    // rtp 封装


    // 判断是否需要ps封装
    if (is_need_ps == true) {
        stream_len = MakePsStream(dst + RTP_HDR_LEN, data_len, frame_info, dst);
    }
    stream_len += RTP_HDR_LEN;
    return stream_len;
}

uint32_t MakePsPacket::MakePsStream(unsigned char * ori_data,
                                    uint64_t data_len,
                                    FrameInfo::Ptr frame_info,
                                    unsigned char * dst) {
    uint32_t head_len = 0;
    unsigned char* head_data = dst;

    // 1 添加ps头
    MakePsHeader(head_data + head_len, frame_info->scr);
    head_len += PS_HDR_LEN;

    if (frame_info->is_i_frame) { // I帧需要添加system头和psm头
        // 2 添加ps_system头
        MakeSysHeader(head_data + head_len);
        head_len += SYS_HDR_LEN;

        // 3 添加ps_psm头
        MakePsmHeader(head_data + head_len);
        head_len += PSM_HDR_LEN;
    }

    // 如果数据长度超过pes负载最大值，则需要打包多个pes
    while (data_len > 0) {
        uint32_t payload_len = data_len;
        if (payload_len > PS_PES_PAYLOAD_SIZE) {
            payload_len = PS_PES_PAYLOAD_SIZE;
        }

        // 4 添加ps_pes头
        MakePesHeader(head_data + head_len, frame_info->stream_id, payload_len, frame_info->pts, frame_info->dts);
        head_len += PES_HDR_LEN;
        memcpy(head_data + head_len, ori_data, payload_len);

        head_len += payload_len; // 已经写入的数据总长度（包含ps相关头字段）

        data_len -= payload_len; // 计算出剩余的数据长度
        ori_data += payload_len; // 指针移向未写入字节
    }

    return head_len;
}

int MakePsPacket::MakeRtpHeader(unsigned char * header_data, int marker_flag, uint16_t cseq, uint32_t pts, uint32_t ssrc) {
    // 初始化bitBuffer
    bits_buffer_s bitsBuffer;
    bitsBuffer.i_size = RTP_HDR_LEN;
    bitsBuffer.p_data = header_data;
    memset(bitsBuffer.p_data, 0, RTP_HDR_LEN);

    bits_write(&bitsBuffer, 2, 0x01);		/* rtp version */
    bits_write(&bitsBuffer, 1, 0x0);		/* rtp padding */
    bits_write(&bitsBuffer, 1, 0x0);		/* rtp extension */
    bits_write(&bitsBuffer, 4, 0x0);		/* rtp CSRC count */
    bits_write(&bitsBuffer, 1, marker_flag);/* rtp marker */
    bits_write(&bitsBuffer, 7, 0x60);		/* rtp payload type*/
    bits_write(&bitsBuffer, 16, cseq);		/* rtp sequence */
    bits_write(&bitsBuffer, 32, pts);		/* rtp timestamp */
    bits_write(&bitsBuffer, 32, ssrc);		/* rtp SSRC */
    return 0;
}

int MakePsPacket::MakePsHeader(unsigned char * header_data, uint64_t scr) {
    // 这里除以100是由于sdp协议返回的video的频率是90000，帧率是25帧/s，所以每次递增的量是3600,
    // 所以实际你应该根据你自己编码里的时间戳来处理以保证时间戳的增量为3600即可，
    // 如果这里不对的话，就可能导致卡顿现象了
    //uint64_t scr_ext = scr % 1000;
    //scr = scr / 1000;

    // 初始化bitBuffer
    bits_buffer_s bitsBuffer;
    bitsBuffer.i_size = PS_HDR_LEN;
    bitsBuffer.p_data = header_data;
    memset(bitsBuffer.p_data, 0, PS_HDR_LEN);

    bits_write(&bitsBuffer, 32, 0x000001BA);            /*start codes*/
    bits_write(&bitsBuffer, 2, 1);					/*marker bits '01b'*/
    bits_write(&bitsBuffer, 3, (scr >> 30) & 0x07);     /*System clock [32..30]*/
    bits_write(&bitsBuffer, 1, 1);					/*marker bit*/
    bits_write(&bitsBuffer, 15, (scr >> 15) & 0x7FFF);	/*System clock [29..15]*/
    bits_write(&bitsBuffer, 1, 1);					/*marker bit*/
    bits_write(&bitsBuffer, 15, scr & 0x7fff);			/*System clock [14..0]*/
    bits_write(&bitsBuffer, 1, 1);					/*marker bit*/
    bits_write(&bitsBuffer, 9, 0);		            /*extern scr*/
    bits_write(&bitsBuffer, 1, 1);					/*marker bit*/
    bits_write(&bitsBuffer, 22, (401410) & 0x3fffff);		/*bit rate(n units of 50 bytes per second.)*/
    bits_write(&bitsBuffer, 2, 3);					/*marker bits '11'*/
    bits_write(&bitsBuffer, 5, 0x1f);					/*reserved(reserved for future use)*/
    bits_write(&bitsBuffer, 3, 0);					/*stuffing length*/
    return 0;
}

int MakePsPacket::MakeSysHeader(unsigned char * header_data) {
    // 初始化bitBuffer
    bits_buffer_s bitsBuffer;
    bitsBuffer.i_size = SYS_HDR_LEN;
    bitsBuffer.p_data = header_data;
    memset(bitsBuffer.p_data, 0, SYS_HDR_LEN);

    /*system header*/
    bits_write(&bitsBuffer, 32, 0x000001BB);		/*start code*/
    bits_write(&bitsBuffer, 16, SYS_HDR_LEN - 6);	/*header_length 表示次字节后面的长度，后面的相关头也是次意思*/
    bits_write(&bitsBuffer, 1, 1);				/*marker_bit*/
    bits_write(&bitsBuffer, 22, 50000);				/*rate_bound*/
    bits_write(&bitsBuffer, 1, 1);				/*marker_bit*/
    bits_write(&bitsBuffer, 6, 1);				/*audio_bound*/
    bits_write(&bitsBuffer, 1, 0);				/*fixed_flag */
    bits_write(&bitsBuffer, 1, 1);				/*CSPS_flag */
    bits_write(&bitsBuffer, 1, 1);				/*system_audio_lock_flag*/
    bits_write(&bitsBuffer, 1, 1);				/*system_video_lock_flag*/
    bits_write(&bitsBuffer, 1, 1);				/*marker_bit*/
    bits_write(&bitsBuffer, 5, 1);				/*video_bound*/
    bits_write(&bitsBuffer, 1, 0);				/*dif from mpeg1*/
    bits_write(&bitsBuffer, 7, 0x7F);				/*reserver*/

    /*video stream bound*/
    bits_write(&bitsBuffer, 8, 0xE0);				/*stream_id*/
    bits_write(&bitsBuffer, 2, 3);					/*marker_bit */
    bits_write(&bitsBuffer, 1, 1);					/*PSTD_buffer_bound_scale*/
    bits_write(&bitsBuffer, 13, 2048);				/*PSTD_buffer_size_bound*/

    /*audio stream bound*/
    bits_write(&bitsBuffer, 8, 0xC0);				/*stream_id*/
    bits_write(&bitsBuffer, 2, 3);					/*marker_bit */
    bits_write(&bitsBuffer, 1, 0);					/*PSTD_buffer_bound_scale*/
    bits_write(&bitsBuffer, 13, 512);				/*PSTD_buffer_size_bound*/

    return 0;
}

int MakePsPacket::MakePsmHeader(unsigned char * header_data) {
    // 初始化bitBuffer
    bits_buffer_s bitsBuffer;
    bitsBuffer.i_size = PSM_HDR_LEN;
    bitsBuffer.p_data = header_data;
    memset(bitsBuffer.p_data, 0, PSM_HDR_LEN);

    memset(bitsBuffer.p_data, 0, PSM_HDR_LEN);
    bits_write(&bitsBuffer, 24, 0x000001);	/*start code*/
    bits_write(&bitsBuffer, 8, 0xBC);		/*map stream id*/
    bits_write(&bitsBuffer, 16, 18);		/*program stream map length*/
    bits_write(&bitsBuffer, 1, 1);       /*current next indicator */
    bits_write(&bitsBuffer, 2, 3);       /*reserved*/
    bits_write(&bitsBuffer, 5, 0);       /*program stream map version*/
    bits_write(&bitsBuffer, 7, 0x7F);       /*reserved */
    bits_write(&bitsBuffer, 1, 1);       /*marker bit */
    bits_write(&bitsBuffer, 16, 0);		/*programe stream info length*/
    bits_write(&bitsBuffer, 16, 8);      /*elementary stream map length  is*/

    /*audio*/
    bits_write(&bitsBuffer, 8, 0x90);       /*stream_type*/
    bits_write(&bitsBuffer, 8, 0xC0);       /*elementary_stream_id*/
    bits_write(&bitsBuffer, 16, 0);         /*elementary_stream_info_length is*/

    /*video*/
    bits_write(&bitsBuffer, 8, 0x1B);       /*stream_type*/
    bits_write(&bitsBuffer, 8, 0xE0);       /*elementary_stream_id*/
    bits_write(&bitsBuffer, 16, 0);         /*elementary_stream_info_length */

    /*crc (2e b9 0f 3d)*/
    bits_write(&bitsBuffer, 8, 0x45);       /*crc (24~31) bits*/
    bits_write(&bitsBuffer, 8, 0xBD);       /*crc (16~23) bits*/
    bits_write(&bitsBuffer, 8, 0xDC);       /*crc (8~15) bits*/
    bits_write(&bitsBuffer, 8, 0xF4);       /*crc (0~7) bits*/
    return 0;
}

int MakePsPacket::MakePesHeader(unsigned char * header_data, int stream_id, int payload_len, uint64_t pts, uint64_t dts) {
    // 初始化bitBuffer
    bits_buffer_s bitsBuffer;
    bitsBuffer.i_size = PES_HDR_LEN;
    bitsBuffer.p_data = header_data;
    memset(bitsBuffer.p_data, 0, PES_HDR_LEN);

    /*system header*/
    bits_write(&bitsBuffer, 24, 0x000001);			/*start code*/
    bits_write(&bitsBuffer, 8, stream_id);			/*streamID*/
    bits_write(&bitsBuffer, 16, (payload_len + 13));  /*packet_len*/ //指出pes分组中数据长度和该字节后的长度和

    bits_write(&bitsBuffer, 2, 2);        /*'10'*/
    bits_write(&bitsBuffer, 2, 0);        /*scrambling_control*/
    bits_write(&bitsBuffer, 1, 0);        /*priority*/
    bits_write(&bitsBuffer, 1, 0);        /*data_alignment_indicator*/
    bits_write(&bitsBuffer, 1, 0);        /*copyright*/
    bits_write(&bitsBuffer, 1, 0);        /*original_or_copy*/

    bits_write(&bitsBuffer, 1, 1);        /*PTS_flag*/
    bits_write(&bitsBuffer, 1, 1);        /*DTS_flag*/
    bits_write(&bitsBuffer, 1, 0);        /*ESCR_flag*/
    bits_write(&bitsBuffer, 1, 0);        /*ES_rate_flag*/
    bits_write(&bitsBuffer, 1, 0);        /*DSM_trick_mode_flag*/
    bits_write(&bitsBuffer, 1, 0);        /*additional_copy_info_flag*/
    bits_write(&bitsBuffer, 1, 0);        /*PES_CRC_flag*/
    bits_write(&bitsBuffer, 1, 0);        /*PES_extension_flag*/

    bits_write(&bitsBuffer, 8, 10);       /*header_data_length*/
    // 指出包含在 PES 分组标题中的可选字段和任何填充字节所占用的总字节数。该字段之前
    //的字节指出了有无可选字段。

    /*PTS,DTS*/
    bits_write(&bitsBuffer, 4, 3);							/*'0011'*/
    bits_write(&bitsBuffer, 3, ((pts) >> 30) & 0x07);		/*PTS[32..30]*/
    bits_write(&bitsBuffer, 1, 1);
    bits_write(&bitsBuffer, 15, ((pts) >> 15) & 0x7FFF);	/*PTS[29..15]*/
    bits_write(&bitsBuffer, 1, 1);
    bits_write(&bitsBuffer, 15, (pts) & 0x7FFF);			/*PTS[14..0]*/
    bits_write(&bitsBuffer, 1, 1);

    bits_write(&bitsBuffer, 4, 1);							/*'0001'*/
    bits_write(&bitsBuffer, 3, ((dts) >> 30) & 0x07);		/*DTS[32..30]*/
    bits_write(&bitsBuffer, 1, 1);
    bits_write(&bitsBuffer, 15, ((dts) >> 15) & 0x7FFF);    /*DTS[29..15]*/
    bits_write(&bitsBuffer, 1, 1);
    bits_write(&bitsBuffer, 15, (dts) & 0x7FFF);			/*DTS[14..0]*/
    bits_write(&bitsBuffer, 1, 1);

    return 0;
}

int MakePsPacket::SetPesPayloadLen(unsigned char * header_data, int payload_len) {
    // 初始化bitBuffer
    bits_buffer_s bitsBuffer;
    bitsBuffer.i_size = PES_HDR_LEN;
    bitsBuffer.p_data = header_data;
    bitsBuffer.i_data = 4; // 第5个字节为长度

    /*system header*/
    bits_write(&bitsBuffer, 16, (payload_len + 13));  /*packet_len*/ //指出pes分组中数据长度和该字节后的长度和
    return 0;
}

int MakePsPacket::SetPesPtsDts(unsigned char * header_data, uint64_t pts, uint64_t dts) {
    // 初始化bitBuffer
    bits_buffer_s bitsBuffer;
    bitsBuffer.i_size = PES_HDR_LEN;
    bitsBuffer.p_data = header_data;
    bitsBuffer.i_data = 9; // 第10~15为pts, 16~20为dts

    /*PTS,DTS*/
    bits_write(&bitsBuffer, 4, 3);							/*'0011'*/
    bits_write(&bitsBuffer, 3, ((pts) >> 30) & 0x07);		/*PTS[32..30]*/
    bits_write(&bitsBuffer, 1, 1);
    bits_write(&bitsBuffer, 15, ((pts) >> 15) & 0x7FFF);	/*PTS[29..15]*/
    bits_write(&bitsBuffer, 1, 1);
    bits_write(&bitsBuffer, 15, (pts) & 0x7FFF);			/*PTS[14..0]*/
    bits_write(&bitsBuffer, 1, 1);

    bits_write(&bitsBuffer, 4, 1);							/*'0001'*/
    bits_write(&bitsBuffer, 3, ((dts) >> 30) & 0x07);		/*DTS[32..30]*/
    bits_write(&bitsBuffer, 1, 1);
    bits_write(&bitsBuffer, 15, ((dts) >> 15) & 0x7FFF);    /*DTS[29..15]*/
    bits_write(&bitsBuffer, 1, 1);
    bits_write(&bitsBuffer, 15, (dts) & 0x7FFF);			/*DTS[14..0]*/
    bits_write(&bitsBuffer, 1, 1);
    return 0;
}
