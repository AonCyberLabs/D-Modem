
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
 *    modem_param.c  --  modem parameters access module.
 *
 *    Author: Sasha K (sashak@smlink.com)
 *
 *
 */


#include <modem.h>
#include <modem_homolog.h>
#include <modem_param.h>



/* param access services */
long modem_get_sreg_param(struct modem *m,unsigned sreg_num)
{
	return modem_get_sreg(m,sreg_num);
}

long modem_get_param(struct modem *m, unsigned param_name)
{
	int val;
	switch (param_name) {
	case MDMPRM_NONE:
		break;
	case MDMPRM_MIN_RATE:
		return m->min_rate;
	case MDMPRM_MAX_RATE:
		return m->max_rate;
	case MDMPRM_IODELAY:
		return m->driver.ioctl(m,MDMCTL_IODELAY,0);
	case MDMPRM_CODECTYPE:
		val = m->driver.ioctl(m,MDMCTL_CODECTYPE,0);
		if(val<0)
			return CODEC_UNKNOWN; /* unknown */
		return val;
	case MDMPRM_DIALSTR:
		return (long)m->dial_string;
	case MDMPRM_AUTOMODE:
		return 0 /* MODEM_AUTOMODE(m)*/;
	case MDMPRM_DPRUNTIME:
		return (long)(m->dp_runtime);
	case MDMPRM_DSPINFO:
		return (long)(&m->dsp_info);
#ifdef MODEM_CONFIG_VOICE
	case MDMPRM_VOICEINFO:
		return (long)(&m->voice_info);
#endif
	case MDMPRM_DP_ADDR:
		return (long)(m->dp);
	case GetPulseDialMakeTime:
		return m->homolog->params->PulseDialMakeTime;
	case GetPulseDialBreakTime:
		return m->homolog->params->PulseDialBreakTime;
	case GetPulseDialDigitPattern:
		return m->homolog->params->PulseDialDigitPattern;
	case GetDTMFHighToneLevel:
		return m->homolog->params->DTMFHighToneLevel;
	case GetDTMFDialSpeed:
		return modem_get_sreg(m,SREG_DTMF_DURATION);
	case GetMinBusyCadenceOnTime:
		return m->homolog->params->MinBusyCadenceOnTime;
	case GetMaxBusyCadenceOnTime:
		return m->homolog->params->MaxBusyCadenceOnTime;
	case GetBusyDetectionCyclesNumber:
		return m->homolog->params->BusyDetectionCyclesNumber;
	case GetMinBusyCadenceOffTime:
		return m->homolog->params->MinBusyCadenceOffTime;
	case GetMaxBusyCadenceOffTime:
		return m->homolog->params->MaxBusyCadenceOffTime;
	case GetCallingToneFlag:
		return m->homolog->params->CallingToneFlag;
	case GetHookFlashTime:
#if 0
		return modem_get_sreg(m,SREG_FLASH_TIMER);
#else
		return m->homolog->params->HookFlashTime;
#endif
	case GetBlindDialPause:
		return modem_get_sreg(m,SREG_DIAL_TONE_WAIT_TIME);
	case GetNoAnswerTimeOut:
		return modem_get_sreg(m,SREG_WAIT_CARRIER_AFTER_DIAL);
	case GetDialPauseTime:
		return modem_get_sreg(m,SREG_DIAL_PAUSE_TIME);
	case GetTransmitLevel:
		return m->homolog->params->TransmitLevel;
	case GetDialModifierValidation:
		return m->homolog->params->DialModifierValidation;
	case GetDialToneValidationTime:
		return m->homolog->params->DialToneValidationTime;
	case GetBusyToneDiffTime:
		return 0;
	case GetDTMFHighAndLowToneLevelDifference:
		return m->homolog->params->DTMFHighAndLowToneLevelDifference;
	case GetPulseDialingFlag:
		return !modem_get_sreg(m,SREG_TONE_OR_PULSE);
	case GetDialToneCallProgressFilterIndex:
		return m->homolog->params->DialToneCallProgressFilterIndex;
	case GetDialToneDetectionThreshold:
		return m->homolog->params->DialToneDetectionThreshold;
	case GetABCDDialingPermittedFlag:
		return m->homolog->params->ABCDDialingPermittedFlag;
	case GetComaPauseDurationLimit:
		return m->homolog->params->ComaPauseDurationLimit;
	case GetPulseAndToneDialInSameDialStringPermittedFlag:
		return 0;
	case GetBusyToneCallProgressFilterIndex:
		return m->homolog->params->BusyToneCallProgressFilterIndex;
	case GetPulseBetweenDigitsInterval:
		return m->homolog->params->PulseBetweenDigitsInterval;
	case GetDialToneWaitTime:
		return modem_get_sreg(m,SREG_DIAL_TONE_WAIT_TIME);
	case GetMinRingbackCadenceOnTime:
		return m->homolog->params->MinRingbackCadenceOnTime;
	case GetMaxRingbackCadenceOnTime:
		return m->homolog->params->MaxRingbackCadenceOnTime;
	case GetRingbackDetectionCyclesNumber:
		return m->homolog->params->RingbackDetectionCyclesNumber;
	case GetMinRingbackCadenceOffTime:
	case GetMaxRingbackCadenceOffTime:
	case GetRingbackToneCallProgressFilterIndex:
	case GetMinCongestionCadenceOnTime:
	case GetMaxCongestionCadenceOnTime:
	case GetCongestionDetectionCyclesNumber:
	case GetMinCongestionCadenceOffTime:
	case GetMaxCongestionCadenceOffTime:
	case GetCongestionToneCallProgressFilterIndex:
		return 0;
	case GetCallProgressSamplesBufferLength:
		return m->homolog->params->CallProgressSamplesBufferLength;
	case MustNoiseFilterBeApplied:
		return m->homolog->params->MustNoiseFilterBeApplied;
	case GetAdditAttenToBeepgenVoice:
		return 0;
	case GetDialToneFilterSubindex:
		return 0;
	case GetBusyToneLooseDetectionEnabled:
		return 0;
	}
	return -1;
}

long modem_set_param(struct modem *m, unsigned name, int val)
{
	switch(name) {
	case MDMPRM_NONE:
		break;
	case MDMPRM_RX_RATE:
		m->rx_rate = val;
		break;
	case MDMPRM_TX_RATE:
		m->tx_rate = val;
		break;
	case MDMPRM_DP_REQUESTED:
		m->dp_requested = val;
		break;
	case MDMPRM_UPDATE_DELAY:
		m->update_delay = val;
		break;
	case MDMPRM_HOOK_ON:
		return m->driver.ioctl(m, MDMCTL_HOOKSTATE,
				       val ? MODEM_HOOK_ON : MODEM_HOOK_OFF );
	case MDMPRM_PULSE_DIAL:
		// bla-bla
		return m->driver.ioctl(m, MDMCTL_PULSEDIAL, val);
	default:
		return -1;
	}
	return 0;
}
