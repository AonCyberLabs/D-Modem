/*
 *
 *    Copyright (c) 2002, Smart Link Ltd.
 *    All rights reserved.
 *
 *    Redistribution and use in source and binary forms, with or without
 *    modification, are permitted provided that the following conditions
 *    are met:
 *
 *        1. Redistributions of source code must retain the above copyright
 *           notice, this list of conditions and the following disclaimer.
 *        2. Redistributions in binary form must reproduce the above
 *           copyright notice, this list of conditions and the following
 *           disclaimer in the documentation and/or other materials provided
 *           with the distribution.
 *        3. Neither the name of the Smart Link Ltd. nor the names of its
 *           contributors may be used to endorse or promote products derived
 *           from this software without specific prior written permission.
 *
 *    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 *
 *    modem_defs.h - modem common definitions
 *
 *    Author: sashak@smlink.com
 */

#ifndef __MODEM_DEFS_H__
#define __MODEM_DEFS_H__

#ifdef __KERNEL__
#include <linux/types.h>
#else

#include <sys/types.h>

typedef __uint8_t  u8;
typedef __uint16_t u16;
typedef __uint32_t u32;

typedef __int8_t  s8;
typedef __int16_t s16;
typedef __int32_t s32;
#endif /* !__KERNEL__ */

/* hook states */
#define MODEM_HOOK_ON       0
#define MODEM_HOOK_OFF      1
#define MODEM_HOOK_SNOOPING 2

/* modem device ctrl interface */
#define MDMCTL_CAPABILITIES  0x01
#define MDMCTL_HOOKSTATE     0x02
#define MDMCTL_SPEED         0x04
#define MDMCTL_GETFMTS       0x05
#define MDMCTL_SETFMT        0x06
#define MDMCTL_SETFRAG       0x07
#define MDMCTL_SETFRAGMENT   MDMCTL_SETFRAG
#define MDMCTL_SPEAKERVOL    0x08
#define MDMCTL_IODELAY       0x10
#define MDMCTL_CODECTYPE     0x11
#define MDMCTL_PULSEDIAL     0x12
#define MDMCTL_GETSTAT       0x20
#define MDMCTL_START         0x21
#define MDMCTL_STOP          0x22


/* modem device status mask */
#define MDMSTAT_ERROR 0x1
#define MDMSTAT_RING  0x2
#define MDMSTAT_DATA  0x4


/* codec types - numbers important */
enum codec_types {
	CODEC_UNKNOWN = 0,
	CODEC_AD1821  = 1,
	CODEC_STLC7550= 4,
	CODEC_SILABS  = 9,
	CODEC_SIL3052 = 10,
	CODEC_SIL3054 = 11,
	CODEC_AD1803  = 12,
	CODEC_ALC545A = 13,
};

/* sample formats + extensions */
#define MFMT_SIZE_MASK   0x000f
#define MFMT_SIZE_8      0x0000
#define MFMT_SIZE_16     0x0001
#define MFMT_SIGN_MASK   0x00f0
#define MFMT_SIGNED      0x0010
#define MFMT_UNSIGNED    0x0000
#define MFMT_ORDER_MASK  0x0f00
#define MFMT_ORDER_LE    0x0100
#define MFMT_ORDER_BE    0x0000

#define MFMT_QUERY       0x0

#define MFMT_S8          (MFMT_SIZE_8|MFMT_SIGNED)
#define MFMT_U8          (MFMT_SIZE_8|MFMT_UNSIGNED)
#define MFMT_S16_LE      (MFMT_SIZE_16|MFMT_SIGNED|MFMT_ORDER_LE)
#define MFMT_S16_BE      (MFMT_SIZE_16|MFMT_SIGNED|MFMT_ORDER_BE)
#define MFMT_U16_LE      (MFMT_SIZE_16|MFMT_UNSIGNED|MFMT_ORDER_LE)
#define MFMT_U16_BE      (MFMT_SIZE_16|MFMT_UNSIGNED|MFMT_ORDER_BE)

#define MFMT_IS_16BIT(x)   ((x)&MFMT_SIZE_16)
#define MFMT_IS_LE(x)      ((x)&MFMT_ORDER_LE)
#define MFMT_BYTESSHIFT(x) ((x)&MFMT_SIZE_MASK)
#define MFMT_SHIFT(x)      (MFMT_BYTESSHIFT(x))
#define MFMT_BYTES(x)      (1<<MFMT_BYTESSHFIT(x))


/* s-registers */
enum S_REGISTER {
        SREG_RINGS_TO_AUTO_ANSWER         =  0,
        SREG_RING_COUNTER                 =  1,
        SREG_ESCAPE_CHAR                  =  2,
        SREG_CR_CHAR                      =  3,
        SREG_LF_CHAR                      =  4,
        SREG_BS_CHAR                      =  5,
        SREG_DIAL_TONE_WAIT_TIME          =  6,
        SREG_WAIT_CARRIER_AFTER_DIAL      =  7,
        SREG_DIAL_PAUSE_TIME              =  8,
        SREG_CARRIER_DETECT_RESPONSE_TIME =  9,
        SREG_CARRIER_LOSS_DISCONNECT_TIME = 10,
        SREG_DTMF_DURATION                = 11,
        SREG_ESCAPE_PROMPT_DELAY          = 12,
        SREG_ECHO                         = 13,
        SREG_QUIET                        = 14,
        SREG_VERBOSE                      = 15,
        SREG_TONE_OR_PULSE                = 16,
        SREG_TEST_TIMER                   = 18,
        SREG_INACTIVITY_TIMEOUT           = 19,
        SREG_BREAK_LENGTH                 = 21,
        SREG_ORIGIN_OR_ANSWER             = 22,
        SREG_XOFF_CHAR                    = 23,
        SREG_FLASH_TIMER                  = 24,
        SREG_DELAY_TO_DTR_OFF             = 25,  /* (NA) */
        SREG_RTS_TO_CTS_DELAY             = 26,  /* (NA) */
        SREG_AUTOANSWER_CLEAR_TIMEOUT     = 27,
        SREG_PULSE_RATIO                  = 28,
        SREG_SPEAKER_CONTROL              = 29,
        SREG_SPEAKER_VOLUME               = 30,
        SREG_SPEAKER_LEVEL                = 30,
        SREG_AUTOMODE                     = 31,
        SREG_DP                           = 32,

#if 1
        SREG_LINE_QUALITY_CONTROL         = 39,
        SREG_X_CODE                       = 56,
        SREG_CD                           = 60,    /* (&CN)    */
        SREG_FLOW_CONTROL                 = 62,    /* (&HN)    */
        SREG_CONNNECT_MSG_FORMAT          = 70,
        SREG_CONNNECT_MSG_SPEED_SRC       = 71,
        SREG_ANS_DELAY                    = 92,
	/* new sgregs */
	SREG_EC                           = 103,
	SREG_COMP                         = 104,

        SREG_DEFAULT_SETTING              = 161, // AT used
#else
        SREG_REQUESTED_DP                 = SREG_DP,
        SREG_REQUESTED_MIN_SPEED          = 33,
        SREG_REQUESTED_MAX_SPEED          = 34,
        SREG_ACTUAL_SPEED                 = 35,
        SREG_CONNECT_RESULT_CODE          = 35,
        SREG_DATA_PUMP_STATUS             = 36,
        SREG_VXX_STATUS                   = 36,
        SREG_ACTUAL_DP                    = 37,
        SREG_ACTUAL_RX_SPEED              = 38,
        SREG_LINE_QUALITY_CONTROL         = 39,
        SREG_SIGNAL_RECEPTION_GAIN        = 41,
        SREG_SNR                          = 42,
        SREG_RESULT_CODES_CONTROL         = 43,
        SREG_CONFIGURATION_SETTING        = 44,
        SREG_SIGNAL_TRANSMISSION_LEVEL    = 45,
        SREG_DISTINCTIVE_RING_SILENCE_PERIOD = 48,
        SREG_ADAPTIVE_ANSWER_MODE            = 49,
        SREG_TEST_BASE                    = 50,
        SREG_TEST_MODE                    = SREG_TEST_BASE,
        SREG_TEST_PAR1                    = (SREG_TEST_BASE+1),
        SREG_MAPPED_S28                   = 52,
        SREG_MAPPED_S37                   = 53,
        SREG_TEST_RES1                    = (SREG_TEST_BASE+4),
        SREG_TEST_RES2                    = (SREG_TEST_BASE+5),
        SREG_X_CODE                       = 56,
        SREG_MESSAGE_DETAIL               = 57,
        SREG_CURRENT_SETTING              = 59,    /* (ZN,&FN) */
        SREG_CD                           = 60,    /* (&CN)    */
        SREG_FLOW_CONTROL                 = 62,    /* (&HN)    */
        SREG_CIRCUIT_106                  = 66,
        SREG_CIRCUIT_107                  = 67,
        SREG_CIRCUIT_109                  = 68,
        SREG_CONNNECT_MSG_FORMAT          = 70,
        SREG_COMMAND_AT_A                 = 70,
        SREG_CONNNECT_MSG_SPEED_SRC       = 71,
        /* Modem Support registers */
        SREG_HANDSET_GANE                 = 72,
        SREG_RECBUF_PURGE_UPON_ABORT      = 73,
        SREG_VOICE_DIALTONE_DETECT_DELAY  = 73,   /* in seconds */
        SREG_PLAYBACK_VALUME              = 74,   /* #VGT */
        SREG_CID_ENABLE                   = 75,   /* #CID */
        SREG_ADPCM_BPS                    = 76,   /* #VBS */
        SREG_BEEP_TONE_TIMER              = 77,   /* #VBT */
        SREG_VOICE_SRC_SELECT             = 78,   /* #VLS */
        SREG_RING_GOES_AWAY_TIMER         = 79,   /* #VRA */
        SREG_RING_NEVER_CAME_TIMER        = 80,   /* #VRN */
        SREG_SILENCE_DETECT_ENABLE        = 81,   /* #VSD */
        SREG_SILENCE_DETECT_SENSITIVITY   = 82,   /* #VSS */
        SREG_SILENCE_DETECT_DURATION      = 83,   /* #VSP */
        SREG_DTMF_TONE_REPORTS_CAP0       = 84,   /* #VTD */
        SREG_DTMF_TONE_REPORTS_CAP1       = 85,
        SREG_DTMF_TONE_REPORTS_CAP2       = 86,
        SREG_TIME_MARK_PLACEMENT          = 87,   /* #VTM   */
        SREG_SPK_MUTE                     = 88,   /* #SPK   */
        SREG_SPK_SPKR                     = 89,   /* #SPK 2 */
        SREG_SPK_MICROPHONE               = 90,   /* #SPK 3 */
        SREG_ADPCM_SAMPLE_RATE            = 91,   /* #VSR   */
        SREG_ANS_DELAY                    = 92,
        /* Voice Modem Support registers */
        SREG_DP_CYCLE_TIME                = 93,
        SREG_ENV_CYCLE_TIME               = 94,
        SREG_V42_CYCLE_TIME               = 95,
        /*  Sample Rate Sregister: */
        SREG_AUDIO_SAMPLE_RATE            = 96,
        SREG_RAW_RMS_ENABLE               = 97,
        SREG_RAW_RMS_IN                   = 98,
        SREG_RAW_RMS_OUT                  = 99,
        SREG_V42_BASEREG                      = 100,
        SREG_V42_DATA_LINK                    = SREG_V42_BASEREG,
        SREG_V42_MNP_BLOCK_SIZE               = (SREG_V42_BASEREG + 1),
        SREG_V42_DATA_COMPRESSION             = (SREG_V42_BASEREG + 2),
        SREG_V42_OUTPUT_PROTOCOL              = (SREG_V42_BASEREG + 24),
        SREG_V42_OUTPUT_COMRESION             = (SREG_V42_BASEREG + 26),
        SREG_V42_ORIG_REQUEST                 = (SREG_V42_BASEREG + 29),
        SREG_V42_ORIG_FALLBACK                = (SREG_V42_BASEREG + 30),
        SREG_V42_ANSWER_FALLBACK              = (SREG_V42_BASEREG + 31),
        SREG_V42_COMPRESSION_RATIO_REPORT     = (SREG_V42_BASEREG + 32),
        SREG_V42_BAD_BYTES_PERCENTAGE         = (SREG_V42_BASEREG + 33),
        SREG_RS_DEBUG_LEVEL                   = (SREG_V42_BASEREG + 34),
        SREG_V42_EA_SINGLE_UNIT_TIME          = (SREG_V42_BASEREG + 35),
        SREG_V42_EA_USED_UNITS_NUMBER         = (SREG_V42_BASEREG + 36),
        SREG_V42_EA_CRITICAL_BAD_UNITS_NUMBER = (SREG_V42_BASEREG + 37),
        SREG_MIC_GAIN                     = 138,
        SREG_LINE_RECORD_GAIN             = 139,
        SREG_LOCAL_FLASH                  = 140,
        SREG_V42_EA_CRITICAL_BAD_UNITS_NUMBER_FOR_V34 = (SREG_V42_BASEREG+41),
        SREG_ATH1_FLAG                    = 142,
        SREG_BELL_MODE                    = 143,
        SREG_MAPPED_S35                   = 144,
        SREG_TEST_COMMAND_SUBMODE         = 145,
        SREG_PULSE_MAKE                   = 146,  /* Percent */
        SREG_PULSE_PPS                    = 147,  /* 10,20   */
        SREG_PULSE_PAUSE                  = 148,  /* * 10ms  */
        SREG_PULSE_REFRESH                = 149,  /* ms      */ 
        SREG_FCLASS_CURRENT               = 150,
        SREG_FAE_CURRENT                  = 151,
        SREG_LINE_OUT_GAIN                = 152,
        SREG_SPK_OUT_GAIN                 = 153,
        SREG_HS_OUT_GAIN                  = 154,
        SREG_DTMF_LOW_GAIN_DB_2SCOMP      = 155,
        SREG_DTMF_HIGH_GAIN_DB_2SCOMP     = 156,
        SREG_SINUS_SAMPLE_RATE_REG_1      = 157,
        SREG_SINUS_SAMPLE_RATE_REG_2      = 158,
        SREG_PULSEDIAL_DISABLE            = 159,
        /* ---------- Unsaved region from 160 ------------------- */
        SREG_DEFAULT_SETTING              = 161, // AT used
        SREG_CALL_PROGRESS_CODE           = 162,
        SREG_COUNTRY_CODE                 = 162,
        SREG_DEBUG_REG_0                  = 170,
        SREG_DEBUG_REG_1                  = 171,
        SREG_DEBUG_REG_2                  = 172,
        SREG_DEBUG_REG_3                  = 173,
        SREG_DEBUG_REG_4                  = 174,
        SREG_DIAG_PROC_CNT1_ID            = 180,
        SREG_DIAG_PROC_CNT2_ID            = 181,
        SREG_DIAG_RING                    = 182,
        SREG_DIAG_COUNT_TYPE              = 183,
        SREG_DIAG_TYPE                    = 184,
        SREG_DIAG_SCOPE                   = 185,
        SREG_PM_UPPER_THR                 = 186,
        SREG_PM_LOWER_THR                 = 187,
        SREG_PM_WEIGHT1                   = 188,
        SREG_PM_WEIGHT2                   = 189,
        SREG_PM_DEGRADE_ENABLE            = 190,
        SREG_PM_DECISION_VALUE            = 191,
        SREG_PROCESSOR_TYPE               = 193,
        SREG_HW_TRACE_LEVEL               = 197,
        SREG_HARDWARE_CONTROL             = 197,
        SREG_STRM_TRACE_LEVEL             = 198,
        SREG_STREAM_MANAGER_DEBUG_LEVEL   = 198,
        SREG_BLACKLISTING_ENABLED_FLAG    = 200,
        SREG_FAILED_ATTEMPTS_BEFORE_BLACKLISTING = 201,
        SREG_BLACKLISTING_DURATION        = 202,
        SREG_BLACKLIST_TIMEOUT            = 203,
        SREG_AT_P_COMMAND_DISABLED        = 204,
        SREG_CID_TIMEOUT_IN_100MS         = 205,
        SREG_V90_PAD_TYPE                 = 210,
        SREG_V90_POWER_REDUCTION          = 211,
        SREG_V90_PROBING_MODE             = 212,
        SREG_V90_COMPUTATIONAL_MODE       = 213,
        SREG_CALL_PROGRESS_SIGNAL_RECEPTION_GAIN = 214,
        SREG_LINE_QUALITY                 = 215,
        SREG_DC_RECALIBRATION_PERIOD      = 216,
        SREG_UPSTREAM_MIN_RATE            = 217,
        SREG_UPSTREAM_MAX_RATE            = 218,
        SREG_V22_LEVEL_CONTROL            = 220,
        SREG_CALLING_TONE_LEVEL_CONTROL   = 221,
#endif
};


/* data pumps identifiers */
enum DP_ID {
        DP_CALLPROG = 2,
        DP_DUMMY = 3,
        DP_AUTOMODE = 4,
        DP_V8 = 8,
	DP_V17 = 17,
	DP_V21 = 21,
        DP_V22 = 22,
        DP_V23 = 23,
        DP_V22BIS = 122,
        DP_V32 = 32,
        DP_V32BIS = 132,
        DP_V34 = 34,
        DP_V34BIS = 134,
        DP_B103 = 103,
        DP_B212 = 212,
        DP_FAX = 100,
        DP_K56 = 56,
        DP_K56_OR_V90 = 65,
        DP_V8BIS = 108,
        DP_V90 = 90,
        DP_V90_NO_V8BIS = 91,
        DP_V92 = 92,
        DP_TEST  = 126,
        DP_SINUS = 127,
	DP_MAX = 128,
        MAX_DP = 128
};

/* dp status */
#define DPSTAT_OK         0
#define DPSTAT_CONNECT    1
#define DPSTAT_ERROR      4
#define DPSTAT_NODIALTONE 6
#define DPSTAT_BUSY       7
#define DPSTAT_NOANSWER   8
#define DPSTAT_CHANGEDP   10

/* dsp info structure */
struct dsp_info {
	unsigned connection_type;
	long     clock_deviation;
	unsigned qc_lapm;
	unsigned qc_index;
};


/* fax definitions */

#define FAX_STATUS_OK        1
#define FAX_STATUS_ERROR     2
#define FAX_STATUS_CONNECT   3
#define FAX_STATUS_NOCARRIER 4

enum FAX_CMD {
	FAX_CMD_FTS,
	FAX_CMD_FRS,
	FAX_CMD_FTM,
	FAX_CMD_FRM,
	FAX_CMD_FTH,
	FAX_CMD_FRH,
};

/* voice definitions */

enum VOICE_CMD {
	VOICE_CMD_STATE_COMMAND,
	VOICE_CMD_STATE_RX,
	VOICE_CMD_STATE_TX,
	VOICE_CMD_STATE_DUPLEX,
	VOICE_CMD_STATE_SPEAKER,
	VOICE_CMD_BEEP,
	VOICE_CMD_DTMF,
	VOICE_CMD_ABORT,
};

#define VOICE_STATUS_OK      1
#define VOICE_STATUS_ERROR   2
#define VOICE_STATUS_CONNECT 3

#define VOICE_STATE_COMMAND 0x00
#define VOICE_STATE_RX      0x01
#define VOICE_STATE_TX      0x02
#define VOICE_STATE_DUPLEX (VOICE_STATE_TX|VOICE_STATE_RX)
#define VOICE_STATE_SPEAKER 0x08

#define VOICE_DEVICE_NONE 0
#define VOICE_DEVICE_LINE 1

struct voice_info {
	unsigned comp_method;
	unsigned sample_rate;
	unsigned rx_gain;
	unsigned tx_gain;
	unsigned dtmf_symbol;
	unsigned tone1_freq;
	unsigned tone2_freq;
	unsigned tone_duration;
	unsigned inactivity_timer;
	unsigned silence_detect_sensitivity;
	unsigned silence_detect_period;
};


/*
 *    prototypes
 *
 */

struct modem;

extern int modem_get_sreg(struct modem *m, unsigned int num);
extern int modem_set_sreg(struct modem *m, unsigned int num, int val);


#endif /* __MODEM_DEFS_H__ */

