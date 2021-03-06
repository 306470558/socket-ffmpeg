// ffmpeg_decoder.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include "pch.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
}
#include <iostream>

#define INBUF_SIZE 4096
FILE *fout;

static void pgm_save(unsigned char *buf, int wrap, int xsize, int ysize, char *filename) {
	
	int i;
	
	fprintf(fout, "P5\n%d %d\n%d\n", xsize, ysize, 255);
	for (i = 0; i < ysize; i++)
		//if you wanted to write out the pixel info into another container, you can do it here.
		//for (int j=0; j<xsize; j++){
		  //std::cout<< int((buf + i * wrap)[j]) <<" ";
		//}
		//instead we write out to a file
		fwrite(buf + i * wrap, 1, xsize, fout);
	
}

static void decode(AVCodecContext *dec_ctx, AVFrame *frame, AVPacket *pkt, const char *filename) {
	char buf[1024];
	int ret;
	//feed input into the decoder
	ret = avcodec_send_packet(dec_ctx, pkt);
	if (ret < 0) {
		fprintf(stderr, "Error sending a packet for decoding\n");
		exit(1);
	}
	while (ret >= 0) {
		//make frames out of it
		ret = avcodec_receive_frame(dec_ctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			return;
		else if (ret < 0) {
			fprintf(stderr, "Error during decoding\n");
			exit(1);
		}
		printf("saving frame %3d\n", dec_ctx->frame_number);
		fflush(stdout);
		/* the picture is allocated by the decoder. no need to
		   free it */
		snprintf(buf, sizeof(buf), "%s-%d", filename, dec_ctx->frame_number);
		pgm_save(frame->data[0], frame->linesize[0],
			frame->width, frame->height, buf);
	}
}

int main(int argc, char **argv) {
	const char *filename, *outfilename;
	const AVCodec *codec;
	AVCodecParserContext *parser;
	AVCodecContext *c = NULL;
	FILE *f;
	AVFrame *frame;
	uint8_t inbuf[INBUF_SIZE + AV_INPUT_BUFFER_PADDING_SIZE];
	uint8_t *data;
	size_t   data_size;
	int ret;
	AVPacket *pkt;
	

	if (argc <= 2) {
		fprintf(stderr, "Usage: %s <input file> <output file>\n", argv[0]);
		exit(0);
	}
	filename = argv[1];
	outfilename = argv[2];
	fout = fopen(outfilename, "w");
 //avcodec_register_all();
 av_register_all();
	pkt = av_packet_alloc();
	if (!pkt)
		exit(1);
	/* set end of buffer to 0 (this ensures that no overreading happens for damaged MPEG streams) */
	memset(inbuf + INBUF_SIZE, 0, AV_INPUT_BUFFER_PADDING_SIZE);
	/* find the H264 video decoder */
	codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!codec) {
		fprintf(stderr, "Codec not found\n");
		exit(1);
	}
	parser = av_parser_init(codec->id);
	if (!parser) {
		fprintf(stderr, "parser not found\n");
		exit(1);
	}
	c = avcodec_alloc_context3(codec);
	if (!c) {
		fprintf(stderr, "Could not allocate video codec context\n");
		exit(1);
	}
	/* For some codecs, such as msmpeg4 and mpeg4, width and height
	   MUST be initialized there because this information is not
	   available in the bitstream. */
	   /* open it */
	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		exit(1);
	}
	f = fopen(filename, "rb");
	if (!f) {
		fprintf(stderr, "Could not open %s\n", filename);
		exit(1);
	}
	frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		exit(1);
	}
	while (!feof(f)) {
		/* read raw data from the input file */
		data_size = fread(inbuf, 1, INBUF_SIZE, f);  //read 1 byte from file, 4096 times, and store it in inbuf
		if (!data_size)
			break;
		/* use the parser to split the data into frames */
		data = inbuf; //data is a pointer to inbuf
		while (data_size > 0) {
			//write the pkt data and pkt size
			ret = av_parser_parse2(parser, c, &pkt->data, &pkt->size,
				data, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
			if (ret < 0) {
				fprintf(stderr, "Error while parsing\n");
				exit(1);
			}
			//the first frame is ret long, so increment the data, and decrease data size.
			data += ret;
			data_size -= ret;
			if (pkt->size)
				decode(c, frame, pkt, outfilename);
		}
	}
	/* flush the decoder */
	decode(c, frame, NULL, outfilename);
	fclose(f);
	av_parser_close(parser);
	avcodec_free_context(&c);
	av_frame_free(&frame);
	av_packet_free(&pkt);
	fclose(fout);
	return 0;
}