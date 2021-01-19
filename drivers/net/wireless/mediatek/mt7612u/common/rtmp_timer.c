/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology	5th	Rd.
 * Science-based Industrial	Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2008, Ralink Technology, Inc.
 *
 * All rights reserved.	Ralink's source code is an unpublished work and	the
 * use of a copyright notice does not imply	otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering,	decoding, reverse engineering or in any
 * way altering the	source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

    Module Name:
    rtmp_timer.c

    Abstract:
    task for timer handling

    Revision History:
    Who         When            What
    --------    ----------      ----------------------------------------------
    Name          Date            Modification logs
    Shiang Tu	08-28-2008   init version

*/

#include "rt_config.h"


BUILD_TIMER_FUNCTION(MlmePeriodicExec);
/*BUILD_TIMER_FUNCTION(MlmeRssiReportExec);*/
BUILD_TIMER_FUNCTION(AsicRxAntEvalTimeout);
BUILD_TIMER_FUNCTION(APSDPeriodicExec);
BUILD_TIMER_FUNCTION(EnqueueStartForPSKExec);
#ifdef CONFIG_STA_SUPPORT
#endif /* CONFIG_STA_SUPPORT */


BUILD_TIMER_FUNCTION(BeaconUpdateExec);

#ifdef CONFIG_AP_SUPPORT
extern VOID APDetectOverlappingExec(
				IN PVOID SystemSpecific1,
				IN PVOID FunctionContext,
				IN PVOID SystemSpecific2,
				IN PVOID SystemSpecific3);

BUILD_TIMER_FUNCTION(APDetectOverlappingExec);

BUILD_TIMER_FUNCTION(Bss2040CoexistTimeOut);

BUILD_TIMER_FUNCTION(GREKEYPeriodicExec);
BUILD_TIMER_FUNCTION(CMTimerExec);
BUILD_TIMER_FUNCTION(WPARetryExec);
#ifdef AP_SCAN_SUPPORT
BUILD_TIMER_FUNCTION(APScanTimeout);
#endif /* AP_SCAN_SUPPORT */
BUILD_TIMER_FUNCTION(APQuickResponeForRateUpExec);
#ifdef DROP_MASK_SUPPORT
BUILD_TIMER_FUNCTION(drop_mask_timer_action);
#endif /* DROP_MASK_SUPPORT */

#endif /* CONFIG_AP_SUPPORT */

#ifdef CONFIG_STA_SUPPORT
BUILD_TIMER_FUNCTION(BeaconTimeout);
BUILD_TIMER_FUNCTION(ScanTimeout);
BUILD_TIMER_FUNCTION(AuthTimeout);
BUILD_TIMER_FUNCTION(AssocTimeout);
BUILD_TIMER_FUNCTION(ReassocTimeout);
BUILD_TIMER_FUNCTION(DisassocTimeout);
BUILD_TIMER_FUNCTION(LinkDownExec);
BUILD_TIMER_FUNCTION(StaQuickResponeForRateUpExec);
BUILD_TIMER_FUNCTION(WpaDisassocApAndBlockAssoc);


BUILD_TIMER_FUNCTION(RtmpUsbStaAsicForceWakeupTimeout);


#endif /* CONFIG_STA_SUPPORT */




BUILD_TIMER_FUNCTION(eTxBfProbeTimerExec);




#ifdef PEER_DELBA_TX_ADAPT
BUILD_TIMER_FUNCTION(PeerDelBATxAdaptTimeOut);
#endif /* PEER_DELBA_TX_ADAPT */
