/*****************************************************************************/

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
 *	modem_homolog.h  --  Modem Homologation definitions.
 *
 *	Author: Sasha K (sashak@smlink.com)
 *
 *
 */

/*****************************************************************************/

#ifndef __MODEM_HOMOLOG_H__
#define __MODEM_HOMOLOG_H__


/* homologation parameters struct */
struct homolog_params {
	u8 PulseDialMakeTime;
	u8 PulseDialBreakTime;
	u8 PulseDialDigitPattern;
	u8 DTMFHighToneLevel;
	u8 DTMFDialSpeed;
	u8 MinBusyCadenceOnTime;
	u8 MaxBusyCadenceOnTime;
	u8 BusyDetectionCyclesNumber;
	u8 MinBusyCadenceOffTime;
	u8 MaxBusyCadenceOffTime;
	u8 CallingToneFlag;
	u8 HookFlashTime;
	u8 DialPauseTime; // ?
	u8 TransmitLevel;
	u8 DialModifierValidation;
	u8 DialToneValidationTime;
	//u8 BusyToneDiffTime;
	u8 DTMFHighAndLowToneLevelDifference;
	u8 DialToneCallProgressFilterIndex;
	u8 DialToneDetectionThreshold;
	u8 ABCDDialingPermittedFlag;
	u8 ComaPauseDurationLimit;
	//u8 PulseAndToneDialInSameDialStringPermittedFlag;
	u8 BusyToneCallProgressFilterIndex;
	u8 PulseBetweenDigitsInterval;
	u8 DialToneWaitTime;
	u8 MinRingbackCadenceOnTime;
	u8 MaxRingbackCadenceOnTime;
	u8 RingbackDetectionCyclesNumber;
	//u8 MinRingbackCadenceOffTime;
	//u8 MaxRingbackCadenceOffTime;
	//u8 RingbackToneCallProgressFilterIndex;
	//u8 MinCongestionCadenceOnTime;
	//u8 MaxCongestionCadenceOnTime;
	//u8 CongestionDetectionCyclesNumber;
	//u8 MinCongestionCadenceOffTime;
	//u8 MaxCongestionCadenceOffTime;
	//u8 CongestionToneCallProgressFilterIndex;
	u16 CallProgressSamplesBufferLength;
	u8 MustNoiseFilterBeApplied;
	//u32 DigitalImpairmentsMask;
	//u8 DialToneFilterSubindex;
	//u8 BusyToneLooseDetectionEnabled;
	//u8 CallWaitingIntegrationTimeMiliSec;
	//u8 CallWaitingThresholdPercentage;
};


/* homologation country set */
struct homolog_set {
	const u16 id;
	const char *name;
	const struct homolog_params *params;
};

/* 'nulled'-terminated homologation array */
extern const struct homolog_set homolog_set[];

#endif /* __MODEM_HOMOLOG_H__ */
