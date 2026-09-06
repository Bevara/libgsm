/*
 *			GPAC - Multimedia Framework C SDK
 *
 *  This file is part of GPAC / GSM 06.10 decoder filter, based on Jutta
 *  Degener and Carsten Bormann's libgsm.
 *
 *  GSM Full Rate is the original digital mobile telephony codec: 8 kHz mono,
 *  160 samples packed into a 33-byte frame at a fixed 13 kbit/s. A raw .gsm
 *  file is nothing but those frames end to end - there is no header, no
 *  container and no other rate - so this filter takes the whole file and
 *  walks it frame by frame.
 */

#include <gpac/filters.h>
#include <gpac/constants.h>
#include <string.h>
#include <stdlib.h>

#include <gsm.h>

/* Fixed by the standard, not read from anywhere. */
#define GSM_FRAME_BYTES 33
#define GSM_FRAME_SAMPLES 160
#define GSM_SAMPLE_RATE 8000

typedef struct
{
	GF_FilterPid *ipid, *opid;
	gsm handle;
} GF_GSMDecCtx;

static GF_Err gsmdec_configure_pid(GF_Filter *filter, GF_FilterPid *pid, Bool is_remove)
{
	GF_GSMDecCtx *ctx = (GF_GSMDecCtx *)gf_filter_get_udta(filter);

	if (is_remove)
	{
		if (ctx->opid)
		{
			gf_filter_pid_remove(ctx->opid);
			ctx->opid = NULL;
		}
		ctx->ipid = NULL;
		return GF_OK;
	}
	if (!gf_filter_pid_check_caps(pid))
		return GF_NOT_SUPPORTED;

	ctx->ipid = pid;
	gf_filter_pid_set_framing_mode(pid, GF_TRUE);

	if (!ctx->opid)
		ctx->opid = gf_filter_pid_new(filter);

	gf_filter_pid_copy_properties(ctx->opid, pid);
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_STREAM_TYPE, &PROP_UINT(GF_STREAM_AUDIO));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CODECID, &PROP_UINT(GF_CODECID_RAW));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_AUDIO_FORMAT, &PROP_UINT(GF_AUDIO_FMT_S16));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_SAMPLE_RATE, &PROP_UINT(GSM_SAMPLE_RATE));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_TIMESCALE, &PROP_UINT(GSM_SAMPLE_RATE));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_NUM_CHANNELS, &PROP_UINT(1));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_CHANNEL_LAYOUT, &PROP_LONGUINT(GF_AUDIO_CH_FRONT_CENTER));
	gf_filter_pid_set_property(ctx->opid, GF_PROP_PID_BITRATE, &PROP_UINT(GSM_SAMPLE_RATE * 16));

	return GF_OK;
}

static GF_Err gsmdec_process(GF_Filter *filter)
{
	GF_FilterPacket *pck, *dst_pck;
	u8 *data, *output;
	u32 size, nb_frames, i;
	GF_GSMDecCtx *ctx = (GF_GSMDecCtx *)gf_filter_get_udta(filter);

	pck = gf_filter_pid_get_packet(ctx->ipid);
	if (!pck)
	{
		if (gf_filter_pid_is_eos(ctx->ipid))
		{
			gf_filter_pid_set_eos(ctx->opid);
			return GF_EOS;
		}
		return GF_OK;
	}
	data = (u8 *)gf_filter_pck_get_data(pck, &size);
	if (!data || (size < GSM_FRAME_BYTES))
	{
		gf_filter_pid_drop_packet(ctx->ipid);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CODEC, ("[GSMDec] File too short to hold a single 33-byte frame\n"));
		return GF_NON_COMPLIANT_BITSTREAM;
	}

	if (!ctx->handle)
	{
		ctx->handle = gsm_create();
		if (!ctx->handle)
		{
			gf_filter_pid_drop_packet(ctx->ipid);
			return GF_OUT_OF_MEM;
		}
	}

	/* A trailing partial frame is ignored rather than padded: it carries no
	 * decodable samples, and guessing at its content would invent audio. */
	nb_frames = size / GSM_FRAME_BYTES;

	dst_pck = gf_filter_pck_new_alloc(ctx->opid, nb_frames * GSM_FRAME_SAMPLES * 2, &output);
	if (!dst_pck)
	{
		gf_filter_pid_drop_packet(ctx->ipid);
		return GF_OUT_OF_MEM;
	}

	for (i = 0; i < nb_frames; i++)
	{
		gsm_signal *out = (gsm_signal *)(output + i * GSM_FRAME_SAMPLES * 2);
		if (gsm_decode(ctx->handle, (gsm_byte *)(data + i * GSM_FRAME_BYTES), out) < 0)
		{
			/* libgsm only rejects a frame whose magic nibble is wrong; the
			 * decoder state stays usable, so the frame is silenced and the
			 * stream carries on rather than aborting the whole file. */
			memset(out, 0, GSM_FRAME_SAMPLES * 2);
			GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[GSMDec] Bad frame %u, silenced\n", i));
		}
	}

	gf_filter_pck_set_cts(dst_pck, 0);
	gf_filter_pck_set_duration(dst_pck, nb_frames * GSM_FRAME_SAMPLES);
	gf_filter_pck_set_sap(dst_pck, GF_FILTER_SAP_1);
	gf_filter_pck_send(dst_pck);

	gf_filter_pid_drop_packet(ctx->ipid);
	gf_filter_pid_set_eos(ctx->opid);
	return GF_EOS;
}

static void gsmdec_finalize(GF_Filter *filter)
{
	GF_GSMDecCtx *ctx = (GF_GSMDecCtx *)gf_filter_get_udta(filter);
	if (ctx->handle)
	{
		gsm_destroy(ctx->handle);
		ctx->handle = NULL;
	}
}

static const GF_FilterCapability GSMDecCaps[] =
	{
		CAP_UINT(GF_CAPS_INPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_FILE),
		CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_FILE_EXT, "gsm"),
		CAP_STRING(GF_CAPS_INPUT, GF_PROP_PID_MIME, "audio/gsm|audio/x-gsm"),
		CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_STREAM_TYPE, GF_STREAM_AUDIO),
		CAP_UINT(GF_CAPS_OUTPUT, GF_PROP_PID_CODECID, GF_CODECID_RAW),
};

GF_FilterRegister GSMDecoderRegister = {
	.name = "gsmdec",
	GF_FS_SET_DESCRIPTION("GSM 06.10 Full Rate decoder")
		GF_FS_SET_HELP("This filter decodes raw GSM 06.10 Full Rate audio (8 kHz mono, 33-byte frames) using libgsm.")
			.private_size = sizeof(GF_GSMDecCtx),
	SETCAPS(GSMDecCaps),
	.configure_pid = gsmdec_configure_pid,
	.process = gsmdec_process,
	.finalize = gsmdec_finalize,
};

const GF_FilterRegister *EMSCRIPTEN_KEEPALIVE gsmdec_register(GF_FilterSession *session)
{
	return &GSMDecoderRegister;
}

#include "filter_register.h"
__attribute__((constructor))
void register_gsmdec(void) {
    gf_filter_auto_register("gsmdec", gsmdec_register);
}
