/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology	5th	Rd.
 * Science-based Industrial	Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2004, Ralink Technology, Inc.
 *
 * All rights reserved.	Ralink's source	code is	an unpublished work	and	the
 * use of a	copyright notice does not imply	otherwise. This	source code
 * contains	confidential trade secret material of Ralink Tech. Any attemp
 * or participation	in deciphering,	decoding, reverse engineering or in	any
 * way altering	the	source code	is stricitly prohibited, unless	the	prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

	Module Name:
	assoc.c

	Abstract:

	Revision History:
	Who			When			What
	--------	----------		----------------------------------------------
	John		2004-9-3		porting from RT2500
*/
#include "rt_config.h"

u8 CipherWpaTemplate[] = {
	0xdd,			/* WPA IE */
	0x16,			/* Length */
	0x00, 0x50, 0xf2, 0x01,	/* oui */
	0x01, 0x00,		/* Version */
	0x00, 0x50, 0xf2, 0x02,	/* Multicast */
	0x01, 0x00,		/* Number of unicast */
	0x00, 0x50, 0xf2, 0x02,	/* unicast */
	0x01, 0x00,		/* number of authentication method */
	0x00, 0x50, 0xf2, 0x01	/* authentication */
};

u8 CipherWpa2Template[] = {
	0x30,			/* RSN IE */
	0x14,			/* Length */
	0x01, 0x00,		/* Version */
	0x00, 0x0f, 0xac, 0x02,	/* group cipher, TKIP */
	0x01, 0x00,		/* number of pairwise */
	0x00, 0x0f, 0xac, 0x02,	/* unicast */
	0x01, 0x00,		/* number of authentication method */
	0x00, 0x0f, 0xac, 0x02,	/* authentication */
	0x00, 0x00,		/* RSN capability */
};


/*
	==========================================================================
	Description:
		Association timeout procedure. After association timeout, this function
		will be called and it will put a message into the MLME queue
	Parameters:
		Standard timer parameters

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID AssocTimeout(
	IN PVOID SystemSpecific1,
	IN PVOID FunctionContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *) FunctionContext;

	/* Do nothing if the driver is starting halt state. */
	/* This might happen when timer already been fired before cancel timer with mlmehalt */
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS | fRTMP_ADAPTER_NIC_NOT_EXIST))
		return;

	MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_ASSOC_TIMEOUT, 0, NULL, 0);
	RTMP_MLME_HANDLER(pAd);
}

/*
	==========================================================================
	Description:
		Reassociation timeout procedure. After reassociation timeout, this
		function will be called and put a message into the MLME queue
	Parameters:
		Standard timer parameters

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID ReassocTimeout(
	IN PVOID SystemSpecific1,
	IN PVOID FunctionContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *) FunctionContext;

	/* Do nothing if the driver is starting halt state. */
	/* This might happen when timer already been fired before cancel timer with mlmehalt */
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS | fRTMP_ADAPTER_NIC_NOT_EXIST))
		return;

	MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_REASSOC_TIMEOUT, 0, NULL, 0);
	RTMP_MLME_HANDLER(pAd);
}

/*
	==========================================================================
	Description:
		Disassociation timeout procedure. After disassociation timeout, this
		function will be called and put a message into the MLME queue
	Parameters:
		Standard timer parameters

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID DisassocTimeout(
	IN PVOID SystemSpecific1,
	IN PVOID FunctionContext,
	IN PVOID SystemSpecific2,
	IN PVOID SystemSpecific3)
{
	struct rtmp_adapter *pAd = (struct rtmp_adapter *) FunctionContext;

	/* Do nothing if the driver is starting halt state. */
	/* This might happen when timer already been fired before cancel timer with mlmehalt */
	if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_HALT_IN_PROGRESS | fRTMP_ADAPTER_NIC_NOT_EXIST))
		return;

	MlmeEnqueue(pAd, ASSOC_STATE_MACHINE, MT2_DISASSOC_TIMEOUT, 0, NULL, 0);
	RTMP_MLME_HANDLER(pAd);
}

/*
	==========================================================================
	Description:
		mlme assoc req handling procedure
	Parameters:
		Adapter - Adapter pointer
		Elem - MLME Queue Element
	Pre:
		the station has been authenticated and the following information is stored in the config
			-# SSID
			-# supported rates and their length
			-# listen interval (Adapter->StaCfg.default_listen_count)
			-# Transmit power  (Adapter->StaCfg.tx_power)
	Post  :
		-# An association request frame is generated and sent to the air
		-# Association timer starts
		-# Association state -> ASSOC_WAIT_RSP

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID MlmeAssocReqAction(
	IN struct rtmp_adapter *pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	ULONG Idx;
	u8 ApAddr[6];
	HEADER_802_11 AssocHdr;
	u8 WmeIe[9] = {IE_VENDOR_SPECIFIC, 0x07, 0x00, 0x50, 0xf2, 0x02, 0x00, 0x01, 0x00};
	unsigned short ListenIntv;
	ULONG Timeout;
	unsigned short CapabilityInfo;
	bool TimerCancelled;
	u8 *pOutBuffer = NULL;
	int NStatus;
	ULONG FrameLen = 0;
	ULONG tmp;
	unsigned short VarIesOffset = 0;
	unsigned short Status;

	/* Block all authentication request durning WPA block period */
	if (pAd->StaCfg.bBlockAssoc == true) {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("ASSOC - Block Assoc request durning WPA block period!\n"));
		pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
		Status = MLME_STATE_MACHINE_REJECT;
		MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_ASSOC_CONF, 2,
			    &Status, 0);
	}
	/* check sanity first */
	else if (MlmeAssocReqSanity(pAd, Elem->Msg, Elem->MsgLen, ApAddr, &CapabilityInfo, &Timeout, &ListenIntv))
	{
		struct rtmp_wifi_dev *wdev = &pAd->StaCfg.wdev;

		/*for dhcp,issue ,wpa_supplicant ioctl too fast , at link_up, it will add key before driver remove key  */
		RTMPWPARemoveAllKeys(pAd);

		RTMPCancelTimer(&pAd->MlmeAux.AssocTimer, &TimerCancelled);
		COPY_MAC_ADDR(pAd->MlmeAux.Bssid, ApAddr);

		/* Get an unused nonpaged memory */
		pOutBuffer = kmalloc(MGMT_DMA_BUFFER_SIZE, GFP_ATOMIC);
		if (pOutBuffer == NULL) {
			DBGPRINT(RT_DEBUG_TRACE,
				 ("ASSOC - MlmeAssocReqAction() allocate memory failed \n"));
			pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
			Status = MLME_FAIL_NO_RESOURCE;
			MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE,
				    MT2_ASSOC_CONF, 2, &Status, 0);
			return;
		}

		/* Add by James 03/06/27 */
		pAd->StaCfg.AssocInfo.Length =
		    sizeof (NDIS_802_11_ASSOCIATION_INFORMATION);
		/* Association don't need to report MAC address */
		pAd->StaCfg.AssocInfo.AvailableRequestFixedIEs =
		    NDIS_802_11_AI_REQFI_CAPABILITIES | NDIS_802_11_AI_REQFI_LISTENINTERVAL;
		pAd->StaCfg.AssocInfo.RequestFixedIEs.Capabilities = CapabilityInfo;
		pAd->StaCfg.AssocInfo.RequestFixedIEs.ListenInterval = ListenIntv;
		/* Only reassociate need this */
		/*COPY_MAC_ADDR(pAd->StaCfg.AssocInfo.RequestFixedIEs.CurrentAPAddress, ApAddr); */
		pAd->StaCfg.AssocInfo.OffsetRequestIEs = sizeof (NDIS_802_11_ASSOCIATION_INFORMATION);

		memset(pAd->StaCfg.ReqVarIEs, 0, MAX_VIE_LEN);
		/* First add SSID */
		VarIesOffset = 0;
		memmove(pAd->StaCfg.ReqVarIEs + VarIesOffset, &SsidIe, 1);
		VarIesOffset += 1;
		memmove(pAd->StaCfg.ReqVarIEs + VarIesOffset, &pAd->MlmeAux.SsidLen, 1);
		VarIesOffset += 1;
		memmove(pAd->StaCfg.ReqVarIEs + VarIesOffset, pAd->MlmeAux.Ssid, pAd->MlmeAux.SsidLen);
		VarIesOffset += pAd->MlmeAux.SsidLen;

		/* Second add Supported rates */
		memmove(pAd->StaCfg.ReqVarIEs + VarIesOffset, &SupRateIe, 1);
		VarIesOffset += 1;
		memmove(pAd->StaCfg.ReqVarIEs + VarIesOffset, &pAd->MlmeAux.SupRateLen, 1);
		VarIesOffset += 1;
		memmove(pAd->StaCfg.ReqVarIEs + VarIesOffset, pAd->MlmeAux.SupRate, pAd->MlmeAux.SupRateLen);
		VarIesOffset += pAd->MlmeAux.SupRateLen;
		/* End Add by James */



		/*
		   CapabilityInfo already sync value with AP in PeerBeaconAtJoinAction.
		   But we need to clean Spectrum Management bit here, if we disable bIEEE80211H in infra sta
		 */
		if (!((pAd->CommonCfg.Channel > 14) &&
		    (pAd->CommonCfg.bIEEE80211H == true))) {
			CapabilityInfo &= (~0x0100);
		}

		DBGPRINT(RT_DEBUG_TRACE, ("%s ASSOC - Send ASSOC request...\n", __FUNCTION__));
		MgtMacHeaderInit(pAd, &AssocHdr, SUBTYPE_ASSOC_REQ, 0, ApAddr,
							pAd->CurrentAddress,
							ApAddr);

		/* Build basic frame first */
		MakeOutgoingFrame(pOutBuffer, &FrameLen,
				  sizeof (HEADER_802_11), &AssocHdr,
				  2, &CapabilityInfo,
				  2, &ListenIntv,
				  1, &SsidIe,
				  1, &pAd->MlmeAux.SsidLen,
				  pAd->MlmeAux.SsidLen, pAd->MlmeAux.Ssid,
				  1, &SupRateIe,
				  1, &pAd->MlmeAux.SupRateLen,
				  pAd->MlmeAux.SupRateLen, pAd->MlmeAux.SupRate,
				  END_OF_ARGS);

		if (pAd->MlmeAux.ExtRateLen != 0) {
			MakeOutgoingFrame(pOutBuffer + FrameLen, &tmp,
					  1, &ExtRateIe,
					  1, &pAd->MlmeAux.ExtRateLen,
					  pAd->MlmeAux.ExtRateLen,
					  pAd->MlmeAux.ExtRate, END_OF_ARGS);
			FrameLen += tmp;
		}

		/* HT */
		if ((pAd->MlmeAux.HtCapabilityLen > 0)
		    && WMODE_CAP_N(pAd->CommonCfg.PhyMode)
		    && pAd->MlmeAux.APEdcaParm.bValid) {
			ULONG TmpLen;
			u8 HtLen, BROADCOM[4] = { 0x0, 0x90, 0x4c, 0x33 };
			PHT_CAPABILITY_IE pHtCapability;
#ifdef __BIG_ENDIAN
			HT_CAPABILITY_IE HtCapabilityTmp;
			memset(&HtCapabilityTmp, 0, sizeof (HT_CAPABILITY_IE));
			memmove(&HtCapabilityTmp, &pAd->MlmeAux.HtCapability, pAd->MlmeAux.HtCapabilityLen);
			*(unsigned short *) (&HtCapabilityTmp.HtCapInfo) = SWAP16(*(unsigned short *) (&HtCapabilityTmp.HtCapInfo));
			*(unsigned short *) (&HtCapabilityTmp.ExtHtCapInfo) = SWAP16(*(unsigned short *) (&HtCapabilityTmp.ExtHtCapInfo));
			pHtCapability = &HtCapabilityTmp;
#else
			pHtCapability = &pAd->MlmeAux.HtCapability;
#endif

			if (pAd->StaActive.SupportedPhyInfo.bPreNHt == true) {
				HtLen = SIZE_HT_CAP_IE + 4;
				MakeOutgoingFrame(pOutBuffer + FrameLen,
						  &TmpLen, 1, &WpaIe, 1, &HtLen,
						  4, &BROADCOM[0],
						  pAd->MlmeAux.HtCapabilityLen,
						  pHtCapability, END_OF_ARGS);
			} else {
				MakeOutgoingFrame(pOutBuffer + FrameLen,
						  &TmpLen, 1, &HtCapIe, 1,
						  &pAd->MlmeAux.HtCapabilityLen,
						  pAd->MlmeAux.HtCapabilityLen,
						  pHtCapability, END_OF_ARGS);
			}
			FrameLen += TmpLen;

			if (WMODE_CAP_AC(pAd->CommonCfg.PhyMode) &&
				(pAd->MlmeAux.Channel > 14) &&
				(pAd->MlmeAux.vht_cap_len)
			)
			{
                //Disable beamform capability in Associate Request with 3x3 AP to avoid throughput drop issue
                // MT76x2 only supports up to 2x2 sounding feedback
                Idx = BssTableSearch(&pAd->ScanTab, pAd->MlmeAux.Bssid, pAd->MlmeAux.Channel);
                if (Idx != BSS_NOT_FOUND)
                {
                    pAd->BeaconSndDimensionFlag = 0;
                    if (pAd->ScanTab.BssEntry[Idx].vht_cap_ie.vht_cap.num_snd_dimension >=2 )
                    {
                        pAd->BeaconSndDimensionFlag = 1;
                    }
                 }
				FrameLen += build_vht_ies(pAd, (u8 *)(pOutBuffer + FrameLen), SUBTYPE_ASSOC_REQ);
			}
		}

		{
			ULONG TmpLen;
			EXT_CAP_INFO_ELEMENT extCapInfo;
			u8 extInfoLen;

			extInfoLen = sizeof (EXT_CAP_INFO_ELEMENT);
			memset(&extCapInfo, 0, extInfoLen);

			if ((pAd->CommonCfg.bBssCoexEnable == true) &&
			    WMODE_CAP_N(pAd->CommonCfg.PhyMode)
			    && (pAd->CommonCfg.Channel <= 14)
			    ) {
				extCapInfo.BssCoexistMgmtSupport = 1;
			}


			if (WMODE_CAP_AC(pAd->CommonCfg.PhyMode) &&
				(pAd->CommonCfg.Channel > 14))
				extCapInfo.operating_mode_notification = 1;


			MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
					1, &ExtCapIe,
					1, &extInfoLen,
					extInfoLen,			&extCapInfo,
					END_OF_ARGS);
			FrameLen += TmpLen;
		}

		/* add Ralink proprietary IE to inform AP this STA is going to use AGGREGATION or PIGGY-BACK+AGGREGATION */
		/* Case I: (Aggregation + Piggy-Back) */
		/* 1. user enable aggregation, AND */
		/* 2. Mac support piggy-back */
		/* 3. AP annouces it's PIGGY-BACK+AGGREGATION-capable in BEACON */
		/* Case II: (Aggregation) */
		/* 1. user enable aggregation, AND */
		/* 2. AP annouces it's AGGREGATION-capable in BEACON */
		if (pAd->CommonCfg.bAggregationCapable) {
			if ((pAd->CommonCfg.bPiggyBackCapable)
			    && ((pAd->MlmeAux.APRalinkIe & 0x00000003) == 3)) {
				ULONG TmpLen;
				u8 RalinkIe[9] = {IE_VENDOR_SPECIFIC, 7, 0x00, 0x0c, 0x43, 0x03, 0x00, 0x00, 0x00};
				MakeOutgoingFrame(pOutBuffer + FrameLen,
						  &TmpLen, 9, RalinkIe,
						  END_OF_ARGS);
				FrameLen += TmpLen;
			} else if (pAd->MlmeAux.APRalinkIe & 0x00000001) {
				ULONG TmpLen;
				u8 RalinkIe[9] = {IE_VENDOR_SPECIFIC, 7, 0x00, 0x0c, 0x43, 0x01, 0x00, 0x00, 0x00};
				MakeOutgoingFrame(pOutBuffer + FrameLen,
						  &TmpLen, 9, RalinkIe,
						  END_OF_ARGS);
				FrameLen += TmpLen;
			}
		} else {
			ULONG TmpLen;
			u8 RalinkIe[9] = {IE_VENDOR_SPECIFIC, 7, 0x00, 0x0c, 0x43, 0x06, 0x00, 0x00, 0x00};
			MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen, 9,
					  RalinkIe, END_OF_ARGS);
			FrameLen += TmpLen;
		}

		if (pAd->MlmeAux.APEdcaParm.bValid) {
			if (pAd->StaCfg.UapsdInfo.bAPSDCapable
			    && pAd->MlmeAux.APEdcaParm.bAPSDCapable) {
				QBSS_STA_INFO_PARM QosInfo;

				memset(&QosInfo, 0, sizeof(QBSS_STA_INFO_PARM));
				QosInfo.UAPSD_AC_BE = pAd->CommonCfg.bAPSDAC_BE;
				QosInfo.UAPSD_AC_BK = pAd->CommonCfg.bAPSDAC_BK;
				QosInfo.UAPSD_AC_VI = pAd->CommonCfg.bAPSDAC_VI;
				QosInfo.UAPSD_AC_VO = pAd->CommonCfg.bAPSDAC_VO;
				QosInfo.MaxSPLength = pAd->CommonCfg.MaxSPLength;
				WmeIe[8] |= *(u8 *) & QosInfo;
			} else {
				/* The Parameter Set Count is set to ??0?? in the association request frames */
				/* WmeIe[8] |= (pAd->MlmeAux.APEdcaParm.EdcaUpdateCount & 0x0f); */
			}

			MakeOutgoingFrame(pOutBuffer + FrameLen, &tmp,
					  9, &WmeIe[0], END_OF_ARGS);
			FrameLen += tmp;
		}

		/*
			Let WPA(#221) Element ID on the end of this association frame.
			Otherwise some AP will fail on parsing Element ID and set status fail on Assoc Rsp.
			For example: Put Vendor Specific IE on the front of WPA IE.
			This happens on AP (Model No:Linksys WRK54G)
		*/
		if (((wdev->AuthMode == Ndis802_11AuthModeWPAPSK) ||
		     (wdev->AuthMode == Ndis802_11AuthModeWPA2PSK) ||
		     (wdev->AuthMode == Ndis802_11AuthModeWPA) ||
		     (wdev->AuthMode == Ndis802_11AuthModeWPA2)
		    )
		    ) {
			u8 RSNIe = IE_WPA;

			if ((wdev->AuthMode == Ndis802_11AuthModeWPA2PSK)
			    || (wdev->AuthMode ==
				Ndis802_11AuthModeWPA2)) {
				RSNIe = IE_WPA2;
			}

#ifdef WPA_SUPPLICANT_SUPPORT
			if (pAd->StaCfg.wpa_supplicant_info.bRSN_IE_FromWpaSupplicant == false)
#endif /* WPA_SUPPLICANT_SUPPORT */
			{
				RTMPMakeRSNIE(pAd, wdev->AuthMode, wdev->WepStatus, BSS0);

				/* Check for WPA PMK cache list */
				if (wdev->AuthMode == Ndis802_11AuthModeWPA2) {
					INT idx;
					bool FoundPMK = false;
					/* Search chched PMKID, append it if existed */
					for (idx = 0; idx < PMKID_NO; idx++) {
						if (!memcmp(ApAddr, &pAd->StaCfg.SavedPMK[idx].BSSID, 6)) {
							FoundPMK = true;
							break;
						}
					}

#ifdef WPA_SUPPLICANT_SUPPORT
					/*
					   When AuthMode is WPA2-Enterprise and AP reboot or STA lost AP,
					   AP would not do PMK cache with STA after STA re-connect to AP again.
					   In this case, driver doesn't need to send PMKID to AP and WpaSupplicant.
					 */
					if ((wdev->AuthMode == Ndis802_11AuthModeWPA2)
					    && (!memcmp(pAd->MlmeAux.Bssid, pAd->CommonCfg.LastBssid, MAC_ADDR_LEN))) {
						FoundPMK = false;
					}
#endif /* WPA_SUPPLICANT_SUPPORT */

					if (FoundPMK) {
						/* Set PMK number */
						*(unsigned short *) & pAd->StaCfg.RSN_IE[pAd->StaCfg.RSNIE_Len] = 1;
						memmove(&pAd->StaCfg.RSN_IE[pAd->StaCfg.RSNIE_Len + 2],
							       &pAd->StaCfg.SavedPMK[idx].PMKID, 16);
						pAd->StaCfg.RSNIE_Len += 18;
					}
				}
			}
#ifdef WPA_SUPPLICANT_SUPPORT
			/*
				Can not use SIOCSIWGENIE definition, it is used in wireless.h
				We will not see the definition in MODULE.
				The definition can be saw in UTIL and NETIF.
			*/
/* #ifdef SIOCSIWGENIE */
			if ((pAd->StaCfg.wpa_supplicant_info.WpaSupplicantUP & WPA_SUPPLICANT_ENABLE)
			    && (pAd->StaCfg.wpa_supplicant_info.bRSN_IE_FromWpaSupplicant == true)) {
				;
			} else
/* #endif */
#endif /* WPA_SUPPLICANT_SUPPORT */
			{
				MakeOutgoingFrame(pOutBuffer + FrameLen, &tmp,
						  1, &RSNIe,
						  1, &pAd->StaCfg.RSNIE_Len,
						  pAd->StaCfg.RSNIE_Len,
						  pAd->StaCfg.RSN_IE,
						  END_OF_ARGS);
				FrameLen += tmp;
			}

#ifdef WPA_SUPPLICANT_SUPPORT
			/*
				Can not use SIOCSIWGENIE definition, it is used in wireless.h
				We will not see the definition in MODULE.
				The definition can be saw in UTIL and NETIF.
			*/
/* #ifdef SIOCSIWGENIE */
			if (((pAd->StaCfg.wpa_supplicant_info.WpaSupplicantUP & 0x7F) !=
			     WPA_SUPPLICANT_ENABLE)
			    || (pAd->StaCfg.wpa_supplicant_info.bRSN_IE_FromWpaSupplicant == false))
/* #endif */
#endif /* WPA_SUPPLICANT_SUPPORT */
			{
				/* Append Variable IE */
				memmove(pAd->StaCfg.ReqVarIEs + VarIesOffset, &RSNIe, 1);
				VarIesOffset += 1;
				memmove(pAd->StaCfg.ReqVarIEs + VarIesOffset, &pAd->StaCfg.RSNIE_Len, 1);
				VarIesOffset += 1;

				memmove(pAd->StaCfg.ReqVarIEs + VarIesOffset, pAd->StaCfg.RSN_IE, pAd->StaCfg.RSNIE_Len);
				VarIesOffset += pAd->StaCfg.RSNIE_Len;

				/* Set Variable IEs Length */
				pAd->StaCfg.ReqVarIELen = VarIesOffset;
			}
		}
#ifdef WPA_SUPPLICANT_SUPPORT
		/*
			Can not use SIOCSIWGENIE definition, it is used in wireless.h
			We will not see the definition in MODULE.
			The definition can be saw in UTIL and NETIF.
		*/
/* #ifdef SIOCSIWGENIE */
		if ((pAd->StaCfg.wpa_supplicant_info.WpaSupplicantUP & WPA_SUPPLICANT_ENABLE) &&
		    (pAd->StaCfg.wpa_supplicant_info.bRSN_IE_FromWpaSupplicant == true)) {
			ULONG TmpWpaAssocIeLen = 0;
			MakeOutgoingFrame(pOutBuffer + FrameLen,
					  &TmpWpaAssocIeLen,
					  pAd->StaCfg.wpa_supplicant_info.WpaAssocIeLen,
					  pAd->StaCfg.wpa_supplicant_info.pWpaAssocIe, END_OF_ARGS);

			FrameLen += TmpWpaAssocIeLen;

			memmove(pAd->StaCfg.ReqVarIEs + VarIesOffset,
				       pAd->StaCfg.wpa_supplicant_info.pWpaAssocIe,
				       pAd->StaCfg.wpa_supplicant_info.WpaAssocIeLen);
			VarIesOffset += pAd->StaCfg.wpa_supplicant_info.WpaAssocIeLen;

			/* Set Variable IEs Length */
			pAd->StaCfg.ReqVarIELen = VarIesOffset;
		}
/* #endif */
#endif /* WPA_SUPPLICANT_SUPPORT */


		MiniportMMRequest(pAd, 0, pOutBuffer, FrameLen);
		kfree(pOutBuffer);

		RTMPSetTimer(&pAd->MlmeAux.AssocTimer, Timeout);
		pAd->Mlme.AssocMachine.CurrState = ASSOC_WAIT_RSP;
	} else {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("ASSOC - MlmeAssocReqAction() sanity check failed. BUG!!!!!! \n"));
		pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
		Status = MLME_INVALID_FORMAT;
		MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_ASSOC_CONF, 2,
			    &Status, 0);
	}

}

/*
	==========================================================================
	Description:
		mlme reassoc req handling procedure
	Parameters:
		Elem -
	Pre:
		-# SSID  (Adapter->StaCfg.ssid[])
		-# BSSID (AP address, Adapter->StaCfg.bssid)
		-# Supported rates (Adapter->StaCfg.supported_rates[])
		-# Supported rates length (Adapter->StaCfg.supported_rates_len)
		-# Tx power (Adapter->StaCfg.tx_power)

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID MlmeReassocReqAction(
	IN struct rtmp_adapter *pAd,
	IN MLME_QUEUE_ELEM * Elem)
{
	ULONG Idx;
	u8 ApAddr[6];
	HEADER_802_11 ReassocHdr;
	u8 WmeIe[9] = {IE_VENDOR_SPECIFIC, 0x07, 0x00, 0x50, 0xf2, 0x02, 0x00, 0x01, 0x00};
	unsigned short CapabilityInfo, ListenIntv;
	ULONG Timeout;
	ULONG FrameLen = 0;
	bool TimerCancelled;
	int NStatus;
	ULONG tmp;
	u8 *pOutBuffer = NULL;
	unsigned short Status;

	/* Block all authentication request durning WPA block period */
	if (pAd->StaCfg.bBlockAssoc == true) {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("ASSOC - Block ReAssoc request durning WPA block period!\n"));
		pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
		Status = MLME_STATE_MACHINE_REJECT;
		MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_REASSOC_CONF, 2, &Status, 0);
	}
	/* the parameters are the same as the association */
	else if (MlmeAssocReqSanity(pAd, Elem->Msg, Elem->MsgLen, ApAddr, &CapabilityInfo, &Timeout, &ListenIntv))
	{
		/*for dhcp,issue ,wpa_supplicant ioctl too fast , at link_up, it will add key before driver remove key  */
		RTMPWPARemoveAllKeys(pAd);

		RTMPCancelTimer(&pAd->MlmeAux.ReassocTimer, &TimerCancelled);

		pOutBuffer = kmalloc(MGMT_DMA_BUFFER_SIZE, GFP_ATOMIC);	/*Get an unused nonpaged memory */
		if (pOutBuffer == NULL) {
			DBGPRINT(RT_DEBUG_TRACE,
				 ("ASSOC - MlmeReassocReqAction() allocate memory failed \n"));
			pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
			Status = MLME_FAIL_NO_RESOURCE;
			MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE,
				    MT2_REASSOC_CONF, 2, &Status, 0);
			return;
		}

		COPY_MAC_ADDR(pAd->MlmeAux.Bssid, ApAddr);

		/* make frame, use bssid as the AP address?? */
		DBGPRINT(RT_DEBUG_TRACE,
			 ("ASSOC - Send RE-ASSOC request...\n"));
		MgtMacHeaderInit(pAd, &ReassocHdr, SUBTYPE_REASSOC_REQ, 0, ApAddr,
							pAd->CurrentAddress,
							ApAddr);
		MakeOutgoingFrame(pOutBuffer, &FrameLen, sizeof (HEADER_802_11),
				  &ReassocHdr, 2, &CapabilityInfo, 2,
				  &ListenIntv, MAC_ADDR_LEN, ApAddr, 1, &SsidIe,
				  1, &pAd->MlmeAux.SsidLen,
				  pAd->MlmeAux.SsidLen, pAd->MlmeAux.Ssid, 1,
				  &SupRateIe, 1, &pAd->MlmeAux.SupRateLen,
				  pAd->MlmeAux.SupRateLen, pAd->MlmeAux.SupRate,
				  END_OF_ARGS);

		if (pAd->MlmeAux.ExtRateLen != 0) {
			MakeOutgoingFrame(pOutBuffer + FrameLen, &tmp,
					  1, &ExtRateIe,
					  1, &pAd->MlmeAux.ExtRateLen,
					  pAd->MlmeAux.ExtRateLen,
					  pAd->MlmeAux.ExtRate, END_OF_ARGS);
			FrameLen += tmp;
		}

		if (pAd->MlmeAux.APEdcaParm.bValid) {
			if (pAd->StaCfg.UapsdInfo.bAPSDCapable
			    && pAd->MlmeAux.APEdcaParm.bAPSDCapable) {
				QBSS_STA_INFO_PARM QosInfo;

				memset(&QosInfo, 0, sizeof (QBSS_STA_INFO_PARM));
				QosInfo.UAPSD_AC_BE = pAd->CommonCfg.bAPSDAC_BE;
				QosInfo.UAPSD_AC_BK = pAd->CommonCfg.bAPSDAC_BK;
				QosInfo.UAPSD_AC_VI = pAd->CommonCfg.bAPSDAC_VI;
				QosInfo.UAPSD_AC_VO = pAd->CommonCfg.bAPSDAC_VO;
				QosInfo.MaxSPLength = pAd->CommonCfg.MaxSPLength;

				DBGPRINT(RT_DEBUG_TRACE, ("uapsd> MaxSPLength = %d!\n", QosInfo.MaxSPLength));
				WmeIe[8] |= *(u8 *) & QosInfo;
			}

			MakeOutgoingFrame(pOutBuffer + FrameLen, &tmp,
					  9, &WmeIe[0], END_OF_ARGS);
			FrameLen += tmp;
		/* HT */
		if ((pAd->MlmeAux.HtCapabilityLen > 0)
		    && WMODE_CAP_N(pAd->CommonCfg.PhyMode)) {
			ULONG TmpLen;
			u8 HtLen;
			u8 BROADCOM[4] = {0x0, 0x90, 0x4c, 0x33};
			PHT_CAPABILITY_IE pHtCapability;

#ifdef __BIG_ENDIAN
			HT_CAPABILITY_IE HtCapabilityTmp;
			memset(&HtCapabilityTmp, 0, sizeof (HT_CAPABILITY_IE));
			memmove(&HtCapabilityTmp, &pAd->MlmeAux.HtCapability, pAd->MlmeAux.HtCapabilityLen);
			*(unsigned short *) (&HtCapabilityTmp.HtCapInfo) = SWAP16(*(unsigned short *) (&HtCapabilityTmp.HtCapInfo));
			*(unsigned short *) (&HtCapabilityTmp.ExtHtCapInfo) = SWAP16(*(unsigned short *) (&HtCapabilityTmp.ExtHtCapInfo));
			pHtCapability = &HtCapabilityTmp;
#else
			pHtCapability = &pAd->MlmeAux.HtCapability;
#endif

			if (pAd->StaActive.SupportedPhyInfo.bPreNHt == true) {
				HtLen = SIZE_HT_CAP_IE + 4;
				MakeOutgoingFrame(pOutBuffer + FrameLen,
						  &TmpLen, 1, &WpaIe, 1, &HtLen,
						  4, &BROADCOM[0],
						  pAd->MlmeAux.HtCapabilityLen,
						  pHtCapability, END_OF_ARGS);
			} else {
				MakeOutgoingFrame(pOutBuffer + FrameLen,
						  &TmpLen, 1, &HtCapIe, 1,
						  &pAd->MlmeAux.HtCapabilityLen,
						  pAd->MlmeAux.HtCapabilityLen,
						  pHtCapability, END_OF_ARGS);
			}
			FrameLen += TmpLen;

			if (WMODE_CAP_AC(pAd->CommonCfg.PhyMode) &&
				(pAd->MlmeAux.Channel > 14) &&
				(pAd->MlmeAux.vht_cap_len)
			)
			{
                 //Disable beamform capability in Associate Request with 3x3 AP to avoid throughput drop issue
                 // MT76x2 only supports up to 2x2 sounding feedback
                 Idx = BssTableSearch(&pAd->ScanTab, pAd->MlmeAux.Bssid, pAd->MlmeAux.Channel);
                 if (Idx != BSS_NOT_FOUND)
                 {
                     pAd->BeaconSndDimensionFlag = 0;
                     if (pAd->ScanTab.BssEntry[Idx].vht_cap_ie.vht_cap.num_snd_dimension >=2 )
                     {
                         pAd->BeaconSndDimensionFlag = 1;
                      }
                  }
				FrameLen += build_vht_ies(pAd, (u8 *)(pOutBuffer + FrameLen), SUBTYPE_ASSOC_REQ);
			}
		}
		} // end of pAd->MlmeAux.APEdcaParm.bValid

		if (false
		 )
		{
			ULONG TmpLen;
			EXT_CAP_INFO_ELEMENT extCapInfo;
			u8 extInfoLen;

			memset(&extCapInfo, 0, sizeof (EXT_CAP_INFO_ELEMENT));
			extInfoLen = sizeof(EXT_CAP_INFO_ELEMENT);



			MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen,
								1,					&ExtCapIe,
								1					&extInfoLen,
								extInfoLen,			&extCapInfo,
								END_OF_ARGS);
			FrameLen += TmpLen;
		}

		/* add Ralink proprietary IE to inform AP this STA is going to use AGGREGATION or PIGGY-BACK+AGGREGATION */
		/* Case I: (Aggregation + Piggy-Back) */
		/* 1. user enable aggregation, AND */
		/* 2. Mac support piggy-back */
		/* 3. AP annouces it's PIGGY-BACK+AGGREGATION-capable in BEACON */
		/* Case II: (Aggregation) */
		/* 1. user enable aggregation, AND */
		/* 2. AP annouces it's AGGREGATION-capable in BEACON */
		if (pAd->CommonCfg.bAggregationCapable) {
			if ((pAd->CommonCfg.bPiggyBackCapable)
			    && ((pAd->MlmeAux.APRalinkIe & 0x00000003) == 3)) {
				ULONG TmpLen;
				u8 RalinkIe[9] = {IE_VENDOR_SPECIFIC, 7, 0x00, 0x0c, 0x43, 0x03, 0x00, 0x00, 0x00 };
				MakeOutgoingFrame(pOutBuffer + FrameLen,
						  &TmpLen, 9, RalinkIe,
						  END_OF_ARGS);
				FrameLen += TmpLen;
			} else if (pAd->MlmeAux.APRalinkIe & 0x00000001) {
				ULONG TmpLen;
				u8 RalinkIe[9] = {IE_VENDOR_SPECIFIC, 7, 0x00, 0x0c, 0x43, 0x01, 0x00, 0x00, 0x00 };
				MakeOutgoingFrame(pOutBuffer + FrameLen,
						  &TmpLen, 9, RalinkIe,
						  END_OF_ARGS);
				FrameLen += TmpLen;
			}
		} else {
			ULONG TmpLen;
			u8 RalinkIe[9] = {IE_VENDOR_SPECIFIC, 7, 0x00, 0x0c, 0x43, 0x04, 0x00, 0x00, 0x00 };
			MakeOutgoingFrame(pOutBuffer + FrameLen, &TmpLen, 9,
					  RalinkIe, END_OF_ARGS);
			FrameLen += TmpLen;
		}

		MiniportMMRequest(pAd, 0, pOutBuffer, FrameLen);
		kfree(pOutBuffer);

		RTMPSetTimer(&pAd->MlmeAux.ReassocTimer, Timeout * 2);	/* in mSec */
		pAd->Mlme.AssocMachine.CurrState = REASSOC_WAIT_RSP;
	} else {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("ASSOC - MlmeReassocReqAction() sanity check failed. BUG!!!! \n"));
		pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
		Status = MLME_INVALID_FORMAT;
		MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_REASSOC_CONF, 2,
			    &Status, 0);
	}
}

/*
	==========================================================================
	Description:
		Upper layer issues disassoc request
	Parameters:
		Elem -

	IRQL = PASSIVE_LEVEL

	==========================================================================
 */
VOID MlmeDisassocReqAction(
	IN struct rtmp_adapter *pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	PMLME_DISASSOC_REQ_STRUCT pDisassocReq;
	HEADER_802_11 DisassocHdr;
	PHEADER_802_11 pDisassocHdr;
	u8 *pOutBuffer = NULL;
	ULONG FrameLen = 0;
	int NStatus;
	bool TimerCancelled;
	ULONG Timeout = 500;
	unsigned short Status;




	/* skip sanity check */
	pDisassocReq = (PMLME_DISASSOC_REQ_STRUCT) (Elem->Msg);

	pOutBuffer = kmalloc(MGMT_DMA_BUFFER_SIZE, GFP_ATOMIC);	/*Get an unused nonpaged memory */
	if (pOutBuffer == NULL) {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("ASSOC - MlmeDisassocReqAction() allocate memory failed\n"));
		pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
		Status = MLME_FAIL_NO_RESOURCE;
		MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_DISASSOC_CONF, 2,
			    &Status, 0);
		return;
	}


	RTMPCancelTimer(&pAd->MlmeAux.DisassocTimer, &TimerCancelled);

	DBGPRINT(RT_DEBUG_TRACE,
		 ("ASSOC - Send DISASSOC request[BSSID::%02x:%02x:%02x:%02x:%02x:%02x (Reason=%d)\n",
		  PRINT_MAC(pDisassocReq->Addr), pDisassocReq->Reason));
	MgtMacHeaderInit(pAd, &DisassocHdr, SUBTYPE_DISASSOC, 0, pDisassocReq->Addr,
						pAd->CurrentAddress,
						pDisassocReq->Addr);	/* patch peap ttls switching issue */
	MakeOutgoingFrame(pOutBuffer, &FrameLen,
			  sizeof (HEADER_802_11), &DisassocHdr,
			  2, &pDisassocReq->Reason, END_OF_ARGS);
	MiniportMMRequest(pAd, 0, pOutBuffer, FrameLen);

	/* To patch Instance and Buffalo(N) AP */
	/* Driver has to send deauth to Instance AP, but Buffalo(N) needs to send disassoc to reset Authenticator's state machine */
	/* Therefore, we send both of them. */
	pDisassocHdr = (PHEADER_802_11) pOutBuffer;
	pDisassocHdr->FC.SubType = SUBTYPE_DEAUTH;
	MiniportMMRequest(pAd, 0, pOutBuffer, FrameLen);

	kfree(pOutBuffer);

	pAd->StaCfg.DisassocReason = REASON_DISASSOC_STA_LEAVING;
	COPY_MAC_ADDR(pAd->StaCfg.DisassocSta, pDisassocReq->Addr);

	RTMPSetTimer(&pAd->MlmeAux.DisassocTimer, Timeout);	/* in mSec */
	pAd->Mlme.AssocMachine.CurrState = DISASSOC_WAIT_RSP;

#ifdef WPA_SUPPLICANT_SUPPORT
#ifndef NATIVE_WPA_SUPPLICANT_SUPPORT
	if (pAd->StaCfg.wpa_supplicant_info.WpaSupplicantUP != WPA_SUPPLICANT_DISABLE) {
		/*send disassociate event to wpa_supplicant */
		RtmpOSWrielessEventSend(pAd->net_dev, RT_WLAN_EVENT_CUSTOM,
					RT_DISASSOC_EVENT_FLAG, NULL, NULL, 0);
	}
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */
#endif /* WPA_SUPPLICANT_SUPPORT */


/* mark here because linkdown also call this function */

	RTMPSendWirelessEvent(pAd, IW_DISASSOC_EVENT_FLAG, NULL, BSS0, 0);

}

/*
	==========================================================================
	Description:
		peer sends assoc rsp back
	Parameters:
		Elme - MLME message containing the received frame

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID PeerAssocRspAction(
	IN struct rtmp_adapter *pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	unsigned short CapabilityInfo, Status, Aid;
	u8 SupRate[MAX_LEN_OF_SUPPORTED_RATES], SupRateLen;
	u8 ExtRate[MAX_LEN_OF_SUPPORTED_RATES], ExtRateLen;
	u8 Addr2[MAC_ADDR_LEN];
	bool TimerCancelled;
	u8 CkipFlag;
	EDCA_PARM EdcaParm;
	HT_CAPABILITY_IE HtCapability;
	ADD_HT_INFO_IE AddHtInfo;	/* AP might use this additional ht info IE */
	u8 HtCapabilityLen = 0;
	u8 AddHtInfoLen;
	u8 NewExtChannelOffset = 0xff;
	EXT_CAP_INFO_ELEMENT ExtCapInfo;
	MAC_TABLE_ENTRY *pEntry;
	IE_LISTS *ie_list = NULL;


	ie_list = kmalloc(sizeof(IE_LISTS), GFP_ATOMIC);
	if (ie_list == NULL) {
		DBGPRINT(RT_DEBUG_OFF, ("%s():mem alloc failed!\n", __FUNCTION__));
		return;
	}
	memset((u8 *)ie_list, 0, sizeof(IE_LISTS));

	if (PeerAssocRspSanity(pAd, Elem->Msg, Elem->MsgLen,
			       Addr2, &CapabilityInfo, &Status, &Aid, SupRate,
			       &SupRateLen, ExtRate, &ExtRateLen, &HtCapability,
			       &AddHtInfo, &HtCapabilityLen, &AddHtInfoLen,
			       &NewExtChannelOffset, &EdcaParm, &ExtCapInfo,
			       &CkipFlag, ie_list))
	{
		/* The frame is for me ? */
		if (MAC_ADDR_EQUAL(Addr2, pAd->MlmeAux.Bssid)) {
			DBGPRINT(RT_DEBUG_TRACE,
				 ("%s():ASSOC - receive ASSOC_RSP to me (status=%d)\n", __FUNCTION__, Status));
			DBGPRINT(RT_DEBUG_TRACE,
				 ("%s():MacTable [%d].AMsduSize = %d. ClientStatusFlags = 0x%lx \n",
				  __FUNCTION__, Elem->Wcid,
				  pAd->MacTab.Content[BSSID_WCID].AMsduSize,
				  pAd->MacTab.Content[BSSID_WCID].ClientStatusFlags));
			RTMPCancelTimer(&pAd->MlmeAux.AssocTimer, &TimerCancelled);



			if (Status == MLME_SUCCESS) {
				u8 MaxSupportedRateIn500Kbps = 0;

				/*
					In roaming case, LinkDown wouldn't be invoked.
					For preventing finding MacTable Hash index malfunction,
					we need to do MacTableDeleteEntry here.
				*/
				pEntry = MacTableLookup(pAd, pAd->CommonCfg.Bssid);
				if (pEntry)
				{
					MacTableDeleteEntry(pAd, pEntry->wcid, pEntry->Addr);
					pEntry = NULL;
				}

				MaxSupportedRateIn500Kbps = dot11_max_sup_rate(SupRateLen, &SupRate[0],
																ExtRateLen, &ExtRate[0]);
				/* go to procedure listed on page 376 */
				AssocPostProc(pAd, Addr2, CapabilityInfo, Aid,
					      SupRate, SupRateLen, ExtRate,
					      ExtRateLen, &EdcaParm,
						ie_list,
					      &HtCapability, HtCapabilityLen,
					      &AddHtInfo);

				StaUpdateMacTableEntry(pAd,
						    &pAd->MacTab.Content[BSSID_WCID],
						    MaxSupportedRateIn500Kbps,
						    &HtCapability,
						    HtCapabilityLen, &AddHtInfo,
						    AddHtInfoLen,
							ie_list,
						    CapabilityInfo);

				RTMPSetSupportMCS(pAd,
								OPMODE_STA,
								&pAd->MacTab.Content[BSSID_WCID],
								SupRate,
								SupRateLen,
								ExtRate,
								ExtRateLen,
								ie_list->vht_cap_len,
								&ie_list->vht_cap,
								&HtCapability,
								HtCapabilityLen);

			}
			pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
			MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_ASSOC_CONF, 2, &Status, 0);

			if (Status == MLME_SUCCESS)
			{
				PFRAME_802_11 pFrame =  (PFRAME_802_11) (Elem->Msg);
				RT_CFG80211_CONN_RESULT_INFORM(pAd, pAd->MlmeAux.Bssid,
                                pAd->StaCfg.ReqVarIEs, pAd->StaCfg.ReqVarIELen,
								&pFrame->Octet[6],
								Elem->MsgLen - 6 - sizeof (HEADER_802_11),
                                true);
			}
		}
	}
	else
	{
		DBGPRINT(RT_DEBUG_TRACE, ("ASSOC - %s() sanity check fail\n", __FUNCTION__));
	}

	if (ie_list != NULL)
		kfree(ie_list);
}


/*
	==========================================================================
	Description:
		peer sends reassoc rsp
	Parametrs:
		Elem - MLME message cntaining the received frame

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID PeerReassocRspAction(
	IN struct rtmp_adapter *pAd,
	IN MLME_QUEUE_ELEM *Elem)
{
	unsigned short CapabilityInfo;
	unsigned short Status;
	unsigned short Aid;
	u8 SupRate[MAX_LEN_OF_SUPPORTED_RATES], SupRateLen;
	u8 ExtRate[MAX_LEN_OF_SUPPORTED_RATES], ExtRateLen;
	u8 Addr2[MAC_ADDR_LEN];
	u8 CkipFlag;
	bool TimerCancelled;
	EDCA_PARM EdcaParm;
	HT_CAPABILITY_IE HtCapability;
	ADD_HT_INFO_IE AddHtInfo;	/* AP might use this additional ht info IE */
	u8 HtCapabilityLen;
	u8 AddHtInfoLen;
	u8 NewExtChannelOffset = 0xff;
	EXT_CAP_INFO_ELEMENT ExtCapInfo;
	IE_LISTS *ie_list = NULL;

	ie_list = kmalloc(sizeof(IE_LISTS), GFP_ATOMIC);
	if (ie_list == NULL) {
		DBGPRINT(RT_DEBUG_OFF, ("%s():mem alloc failed!\n", __FUNCTION__));
		return;
	}
	memset((u8 *)ie_list, 0, sizeof(IE_LISTS));

	if (PeerAssocRspSanity(pAd, Elem->Msg, Elem->MsgLen, Addr2,
			       &CapabilityInfo, &Status, &Aid, SupRate,
			       &SupRateLen, ExtRate, &ExtRateLen, &HtCapability,
			       &AddHtInfo, &HtCapabilityLen, &AddHtInfoLen,
			       &NewExtChannelOffset, &EdcaParm, &ExtCapInfo,
			       &CkipFlag, ie_list)) {
		if (MAC_ADDR_EQUAL(Addr2, pAd->MlmeAux.Bssid)) {	/* The frame is for me ? */
			DBGPRINT(RT_DEBUG_TRACE,
				 ("REASSOC - receive REASSOC_RSP to me (status=%d)\n", Status));
			RTMPCancelTimer(&pAd->MlmeAux.ReassocTimer,
					&TimerCancelled);

			if (Status == MLME_SUCCESS)
			{
				u8 MaxSupportedRateIn500Kbps = 0;
				PMAC_TABLE_ENTRY pEntry = NULL;

				/*
					In roaming case, LinkDown wouldn't be invoked.
					For preventing finding MacTable Hash index malfunction,
					we need to do MacTableDeleteEntry here.
				*/
				pEntry = MacTableLookup(pAd, pAd->CommonCfg.Bssid);
				if (pEntry)
				{
					MacTableDeleteEntry(pAd, pEntry->wcid, pEntry->Addr);
					pEntry = NULL;
				}

				MaxSupportedRateIn500Kbps = dot11_max_sup_rate(SupRateLen, &SupRate[0], ExtRateLen, &ExtRate[0]);


				/* go to procedure listed on page 376 */
				AssocPostProc(pAd, Addr2, CapabilityInfo, Aid,
					      SupRate, SupRateLen, ExtRate,
					      ExtRateLen, &EdcaParm,
						ie_list,
					      &HtCapability, HtCapabilityLen,
					      &AddHtInfo);

				StaUpdateMacTableEntry(pAd,
						    &pAd->MacTab.Content[BSSID_WCID],
						    MaxSupportedRateIn500Kbps,
						    &HtCapability,
						    HtCapabilityLen, &AddHtInfo,
						    AddHtInfoLen,
							ie_list,
						    CapabilityInfo);

				RTMPSetSupportMCS(pAd,
								OPMODE_STA,
								&pAd->MacTab.Content[BSSID_WCID],
								SupRate,
								SupRateLen,
								ExtRate,
								ExtRateLen,
								ie_list->vht_cap_len,
								&ie_list->vht_cap,
								&HtCapability,
								HtCapabilityLen);

#ifdef WPA_SUPPLICANT_SUPPORT
#ifndef NATIVE_WPA_SUPPLICANT_SUPPORT
				if (pAd->StaCfg.wpa_supplicant_info.WpaSupplicantUP != WPA_SUPPLICANT_DISABLE) {
					SendAssocIEsToWpaSupplicant(pAd->net_dev,
								    pAd->StaCfg.ReqVarIEs,
								    pAd->StaCfg.ReqVarIELen);
					RtmpOSWrielessEventSend(pAd->net_dev,
								RT_WLAN_EVENT_CUSTOM,
								RT_ASSOC_EVENT_FLAG,
								NULL, NULL, 0);
				}
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */
#endif /* WPA_SUPPLICANT_SUPPORT */

#ifdef NATIVE_WPA_SUPPLICANT_SUPPORT
				{
					wext_notify_event_assoc(pAd->net_dev,
								pAd->StaCfg.ReqVarIEs,
								pAd->StaCfg.ReqVarIELen);
					RtmpOSWrielessEventSend(pAd->net_dev,
								RT_WLAN_EVENT_CGIWAP,
								-1,
								&pAd->MlmeAux.Bssid[0], NULL,
								0);
				}
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */

			}

			/* CkipFlag is no use for reassociate */
			pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
			MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_REASSOC_CONF, 2, &Status, 0);
		}
	}
	else
	{
		DBGPRINT(RT_DEBUG_TRACE,
			 ("REASSOC - %s() sanity check fail\n", __FUNCTION__));
	}

	if (ie_list)
		kfree(ie_list);

}

/*
	==========================================================================
	Description:
		procedures on IEEE 802.11/1999 p.376
	Parametrs:

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID AssocPostProc(
	IN struct rtmp_adapter *pAd,
	IN u8 *pAddr2,
	IN unsigned short CapabilityInfo,
	IN unsigned short Aid,
	IN u8 SupRate[],
	IN u8 SupRateLen,
	IN u8 ExtRate[],
	IN u8 ExtRateLen,
	IN PEDCA_PARM pEdcaParm,
	IN IE_LISTS *ie_list,
	IN HT_CAPABILITY_IE *pHtCapability,
	IN u8 HtCapabilityLen,
	IN ADD_HT_INFO_IE *pAddHtInfo)
{				/* AP might use this additional ht info IE */
	ULONG Idx;
	MAC_TABLE_ENTRY *pEntry;

	pAd->MlmeAux.BssType = BSS_INFRA;
	COPY_MAC_ADDR(pAd->MlmeAux.Bssid, pAddr2);
	pAd->MlmeAux.Aid = Aid;
	pAd->MlmeAux.CapabilityInfo = CapabilityInfo & SUPPORTED_CAPABILITY_INFO;

	/* Some HT AP might lost WMM IE. We add WMM ourselves. beacuase HT requires QoS on. */
	if ((HtCapabilityLen > 0) && (pEdcaParm->bValid == false)) {
		pEdcaParm->bValid = true;
		pEdcaParm->Aifsn[0] = 3;
		pEdcaParm->Aifsn[1] = 7;
		pEdcaParm->Aifsn[2] = 2;
		pEdcaParm->Aifsn[3] = 2;

		pEdcaParm->Cwmin[0] = 4;
		pEdcaParm->Cwmin[1] = 4;
		pEdcaParm->Cwmin[2] = 3;
		pEdcaParm->Cwmin[3] = 2;

		pEdcaParm->Cwmax[0] = 10;
		pEdcaParm->Cwmax[1] = 10;
		pEdcaParm->Cwmax[2] = 4;
		pEdcaParm->Cwmax[3] = 3;

		pEdcaParm->Txop[0] = 0;
		pEdcaParm->Txop[1] = 0;
		pEdcaParm->Txop[2] = 96;
		pEdcaParm->Txop[3] = 48;

	}

	memmove(&pAd->MlmeAux.APEdcaParm, pEdcaParm, sizeof (EDCA_PARM));

	/* filter out un-supported rates */
	pAd->MlmeAux.SupRateLen = SupRateLen;
	memmove(pAd->MlmeAux.SupRate, SupRate, SupRateLen);
	RTMPCheckRates(pAd, pAd->MlmeAux.SupRate, &pAd->MlmeAux.SupRateLen);

	/* filter out un-supported rates */
	pAd->MlmeAux.ExtRateLen = ExtRateLen;
	memmove(pAd->MlmeAux.ExtRate, ExtRate, ExtRateLen);
	RTMPCheckRates(pAd, pAd->MlmeAux.ExtRate, &pAd->MlmeAux.ExtRateLen);

	pEntry = &pAd->MacTab.Content[BSSID_WCID];
	if (HtCapabilityLen > 0) {
		RTMPCheckHt(pAd, BSSID_WCID, pHtCapability, pAddHtInfo);
	}
	DBGPRINT(RT_DEBUG_TRACE,
		 ("%s():=>AP.AMsduSize = %d. ClientStatusFlags = 0x%lx \n",
			__FUNCTION__, pEntry->AMsduSize, pEntry->ClientStatusFlags));

	DBGPRINT(RT_DEBUG_TRACE,
		 ("%s():=>(Mmps=%d, AmsduSize=%d, )\n",
		 	__FUNCTION__, pEntry->MmpsMode, pEntry->AMsduSize));

	if (ie_list->vht_cap_len > 0 && ie_list->vht_op_len > 0) {
		RTMPCheckVht(pAd, BSSID_WCID, &ie_list->vht_cap, &ie_list->vht_op);
	}

	/* Set New WPA information */
	Idx = BssTableSearch(&pAd->ScanTab, pAddr2, pAd->MlmeAux.Channel);
	if (Idx == BSS_NOT_FOUND) {
		DBGPRINT_ERR(("ASSOC - Can't find BSS after receiving Assoc response\n"));
	} else {
		struct rtmp_wifi_dev *wdev = &pAd->StaCfg.wdev;
		/* Init variable */
		pEntry->RSNIE_Len = 0;
		memset(pEntry->RSN_IE, 0, MAX_LEN_OF_RSNIE);

		/* Store appropriate RSN_IE for WPA SM negotiation later */
		if ((wdev->AuthMode >= Ndis802_11AuthModeWPA)
		    && (pAd->ScanTab.BssEntry[Idx].VarIELen != 0)) {
			u8 *pVIE;
			unsigned short len;
			PEID_STRUCT pEid;

			pVIE = pAd->ScanTab.BssEntry[Idx].VarIEs;
			len = pAd->ScanTab.BssEntry[Idx].VarIELen;

			while (len > 0) {
				pEid = (PEID_STRUCT) pVIE;
				/* For WPA/WPAPSK */
				if ((pEid->Eid == IE_WPA)
				    && (!memcmp(pEid->Octet, WPA_OUI, 4))
				    && (wdev->AuthMode == Ndis802_11AuthModeWPA
					|| wdev->AuthMode == Ndis802_11AuthModeWPAPSK)) {
					memmove(pEntry->RSN_IE, pVIE, (pEid->Len + 2));
					pEntry->RSNIE_Len = (pEid->Len + 2);
					DBGPRINT(RT_DEBUG_TRACE,
						 ("%s():=> Store RSN_IE for WPA SM negotiation\n", __FUNCTION__));
				}
				/* For WPA2/WPA2PSK */
				else if ((pEid->Eid == IE_RSN)
					 && (!memcmp(pEid->Octet + 2, RSN_OUI, 3))
					 && (wdev->AuthMode == Ndis802_11AuthModeWPA2
					     || wdev->AuthMode == Ndis802_11AuthModeWPA2PSK)) {
					memmove(pEntry->RSN_IE, pVIE, (pEid->Len + 2));
					pEntry->RSNIE_Len = (pEid->Len + 2);
					DBGPRINT(RT_DEBUG_TRACE,
						 ("%s():=> Store RSN_IE for WPA2 SM negotiation\n", __FUNCTION__));
				}

				pVIE += (pEid->Len + 2);
				len -= (pEid->Len + 2);
			}


		}

		if (pAd->MacTab.Content[BSSID_WCID].RSNIE_Len == 0) {
			DBGPRINT(RT_DEBUG_TRACE, ("%s():=> no RSN_IE\n", __FUNCTION__));
		} else {
			hex_dump("RSN_IE", pEntry->RSN_IE, pEntry->RSNIE_Len);
		}
	}
}

/*
	==========================================================================
	Description:
		left part of IEEE 802.11/1999 p.374
	Parameters:
		Elem - MLME message containing the received frame

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID PeerDisassocAction(struct rtmp_adapter *pAd, MLME_QUEUE_ELEM *Elem)
{
	u8 Addr2[MAC_ADDR_LEN];
	unsigned short Reason;

	DBGPRINT(RT_DEBUG_TRACE, ("ASSOC - PeerDisassocAction()\n"));
	if (PeerDisassocSanity(pAd, Elem->Msg, Elem->MsgLen, Addr2, &Reason)) {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("ASSOC - PeerDisassocAction() Reason = %d\n",
			  Reason));
		if (INFRA_ON(pAd)
		    && MAC_ADDR_EQUAL(pAd->CommonCfg.Bssid, Addr2)) {

			RTMPSendWirelessEvent(pAd, IW_DISASSOC_EVENT_FLAG, NULL,
					      BSS0, 0);


			/*
			   It is possible that AP sends dis-assoc frame(PeerDisassocAction) to STA
			   after driver enqueue MT2_MLME_DISASSOC_REQ (MlmeDisassocReqAction)
			   and set CntlMachine.CurrState = CNTL_WAIT_DISASSOC.
			   DisassocTimer is useless because AssocMachine.CurrState will set to ASSOC_IDLE here.
			   Therefore, we need to check CntlMachine.CurrState here and enqueue MT2_DISASSOC_CONF to
			   reset CntlMachine.CurrState to CNTL_IDLE state again.
			 */
			if (pAd->Mlme.CntlMachine.CurrState ==
			    CNTL_WAIT_DISASSOC) {
				unsigned short Status;
				Status = MLME_SUCCESS;
				MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE,
					    MT2_DISASSOC_CONF, 2, &Status, 0);
			} else
				LinkDown(pAd, true);

			pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;

#ifdef WPA_SUPPLICANT_SUPPORT
#ifndef NATIVE_WPA_SUPPLICANT_SUPPORT
			if (pAd->StaCfg.wpa_supplicant_info.WpaSupplicantUP != WPA_SUPPLICANT_DISABLE) {
				/*send disassociate event to wpa_supplicant */
				RtmpOSWrielessEventSend(pAd->net_dev,
							RT_WLAN_EVENT_CUSTOM,
							RT_DISASSOC_EVENT_FLAG,
							NULL, NULL, 0);
			}
#endif /* NATIVE_WPA_SUPPLICANT_SUPPORT */
#endif /* WPA_SUPPLICANT_SUPPORT */
/* mark here because linkdown also call this function */
		}
	} else {
		DBGPRINT(RT_DEBUG_TRACE,
			 ("ASSOC - PeerDisassocAction() sanity check fail\n"));
	}

}

/*
	==========================================================================
	Description:
		what the state machine will do after assoc timeout
	Parameters:
		Elme -

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID AssocTimeoutAction(struct rtmp_adapter *pAd, MLME_QUEUE_ELEM *Elem)
{
	unsigned short Status;
	DBGPRINT(RT_DEBUG_TRACE, ("ASSOC - AssocTimeoutAction\n"));
	pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
	Status = MLME_REJ_TIMEOUT;
	MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_ASSOC_CONF, 2, &Status,
		    0);
}

/*
	==========================================================================
	Description:
		what the state machine will do after reassoc timeout

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID ReassocTimeoutAction(struct rtmp_adapter *pAd, MLME_QUEUE_ELEM *Elem)
{
	unsigned short Status;
	DBGPRINT(RT_DEBUG_TRACE, ("ASSOC - ReassocTimeoutAction\n"));
	pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
	Status = MLME_REJ_TIMEOUT;
	MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_REASSOC_CONF, 2, &Status,
		    0);
}

/*
	==========================================================================
	Description:
		what the state machine will do after disassoc timeout

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID DisassocTimeoutAction(struct rtmp_adapter *pAd, MLME_QUEUE_ELEM *Elem)
{
	unsigned short Status;
	DBGPRINT(RT_DEBUG_TRACE, ("ASSOC - DisassocTimeoutAction\n"));
	pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
	Status = MLME_SUCCESS;
	MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_DISASSOC_CONF, 2, &Status,
		    0);
}

VOID InvalidStateWhenAssoc(struct rtmp_adapter *pAd, MLME_QUEUE_ELEM *Elem)
{
	unsigned short Status;
	DBGPRINT(RT_DEBUG_TRACE,
		 ("ASSOC - InvalidStateWhenAssoc(state=%ld), reset ASSOC state machine\n",
		  pAd->Mlme.AssocMachine.CurrState));
	pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
	Status = MLME_STATE_MACHINE_REJECT;
	MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_ASSOC_CONF, 2, &Status,
		    0);
}

VOID InvalidStateWhenReassoc(struct rtmp_adapter *pAd, MLME_QUEUE_ELEM *Elem)
{
	unsigned short Status;
	DBGPRINT(RT_DEBUG_TRACE,
		 ("ASSOC - InvalidStateWhenReassoc(state=%ld), reset ASSOC state machine\n",
		  pAd->Mlme.AssocMachine.CurrState));
	pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
	Status = MLME_STATE_MACHINE_REJECT;
	MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_REASSOC_CONF, 2, &Status,
		    0);
}

VOID InvalidStateWhenDisassociate(struct rtmp_adapter *pAd, MLME_QUEUE_ELEM *Elem)
{
	unsigned short Status;
	DBGPRINT(RT_DEBUG_TRACE,
		 ("ASSOC - InvalidStateWhenDisassoc(state=%ld), reset ASSOC state machine\n",
		  pAd->Mlme.AssocMachine.CurrState));
	pAd->Mlme.AssocMachine.CurrState = ASSOC_IDLE;
	Status = MLME_STATE_MACHINE_REJECT;
	MlmeEnqueue(pAd, MLME_CNTL_STATE_MACHINE, MT2_DISASSOC_CONF, 2, &Status,
		    0);
}

/*
	==========================================================================
	Description:
		right part of IEEE 802.11/1999 page 374
	Note:
		This event should never cause ASSOC state machine perform state
		transition, and has no relationship with CNTL machine. So we separate
		this routine as a service outside of ASSOC state transition table.

	IRQL = DISPATCH_LEVEL

	==========================================================================
 */
VOID Cls3errAction(struct rtmp_adapter *pAd, u8 *pAddr)
{
	HEADER_802_11 DisassocHdr;
	PHEADER_802_11 pDisassocHdr;
	u8 *pOutBuffer = NULL;
	ULONG FrameLen = 0;
	int NStatus;
	unsigned short Reason = REASON_CLS3ERR;

	pOutBuffer = kmalloc(MGMT_DMA_BUFFER_SIZE, GFP_ATOMIC);	/*Get an unused nonpaged memory */
	if (pOutBuffer == NULL)
		return;

	DBGPRINT(RT_DEBUG_TRACE,
		 ("ASSOC - Class 3 Error, Send DISASSOC frame\n"));
	MgtMacHeaderInit(pAd, &DisassocHdr, SUBTYPE_DISASSOC, 0, pAddr,
						pAd->CurrentAddress,
						pAd->CommonCfg.Bssid);	/* patch peap ttls switching issue */
	MakeOutgoingFrame(pOutBuffer, &FrameLen,
			  sizeof (HEADER_802_11), &DisassocHdr,
			  2, &Reason, END_OF_ARGS);
	MiniportMMRequest(pAd, 0, pOutBuffer, FrameLen);

	/* To patch Instance and Buffalo(N) AP */
	/* Driver has to send deauth to Instance AP, but Buffalo(N) needs to send disassoc to reset Authenticator's state machine */
	/* Therefore, we send both of them. */
	pDisassocHdr = (PHEADER_802_11) pOutBuffer;
	pDisassocHdr->FC.SubType = SUBTYPE_DEAUTH;
	MiniportMMRequest(pAd, 0, pOutBuffer, FrameLen);

	kfree(pOutBuffer);

	pAd->StaCfg.DisassocReason = REASON_CLS3ERR;
	COPY_MAC_ADDR(pAd->StaCfg.DisassocSta, pAddr);
}


/*
	==========================================================================
	Description:
		association state machine init, including state transition and timer init
	Parameters:
		S - pointer to the association state machine

	IRQL = PASSIVE_LEVEL

	==========================================================================
 */
VOID AssocStateMachineInit(
	IN struct rtmp_adapter *pAd,
	IN STATE_MACHINE *S,
	OUT STATE_MACHINE_FUNC Trans[])
{
	StateMachineInit(S, Trans, MAX_ASSOC_STATE, MAX_ASSOC_MSG,
			 (STATE_MACHINE_FUNC) Drop, ASSOC_IDLE,
			 ASSOC_MACHINE_BASE);

	/* first column */
	StateMachineSetAction(S, ASSOC_IDLE, MT2_MLME_ASSOC_REQ,
			      (STATE_MACHINE_FUNC) MlmeAssocReqAction);
	StateMachineSetAction(S, ASSOC_IDLE, MT2_MLME_REASSOC_REQ,
			      (STATE_MACHINE_FUNC) MlmeReassocReqAction);
	StateMachineSetAction(S, ASSOC_IDLE, MT2_MLME_DISASSOC_REQ,
			      (STATE_MACHINE_FUNC) MlmeDisassocReqAction);
	StateMachineSetAction(S, ASSOC_IDLE, MT2_PEER_DISASSOC_REQ,
			      (STATE_MACHINE_FUNC) PeerDisassocAction);

	/* second column */
	StateMachineSetAction(S, ASSOC_WAIT_RSP, MT2_MLME_ASSOC_REQ,
			      (STATE_MACHINE_FUNC) InvalidStateWhenAssoc);
	StateMachineSetAction(S, ASSOC_WAIT_RSP, MT2_MLME_REASSOC_REQ,
			      (STATE_MACHINE_FUNC) InvalidStateWhenReassoc);
	StateMachineSetAction(S, ASSOC_WAIT_RSP, MT2_MLME_DISASSOC_REQ,
			      (STATE_MACHINE_FUNC)
			      InvalidStateWhenDisassociate);
	StateMachineSetAction(S, ASSOC_WAIT_RSP, MT2_PEER_DISASSOC_REQ,
			      (STATE_MACHINE_FUNC) PeerDisassocAction);
	StateMachineSetAction(S, ASSOC_WAIT_RSP, MT2_PEER_ASSOC_RSP,
			      (STATE_MACHINE_FUNC) PeerAssocRspAction);
	/* */
	/* Patch 3Com AP MOde:3CRWE454G72 */
	/* We send Assoc request frame to this AP, it always send Reassoc Rsp not Associate Rsp. */
	/* */
	StateMachineSetAction(S, ASSOC_WAIT_RSP, MT2_PEER_REASSOC_RSP,
			      (STATE_MACHINE_FUNC) PeerAssocRspAction);
	StateMachineSetAction(S, ASSOC_WAIT_RSP, MT2_ASSOC_TIMEOUT,
			      (STATE_MACHINE_FUNC) AssocTimeoutAction);

	/* third column */
	StateMachineSetAction(S, REASSOC_WAIT_RSP, MT2_MLME_ASSOC_REQ,
			      (STATE_MACHINE_FUNC) InvalidStateWhenAssoc);
	StateMachineSetAction(S, REASSOC_WAIT_RSP, MT2_MLME_REASSOC_REQ,
			      (STATE_MACHINE_FUNC) InvalidStateWhenReassoc);
	StateMachineSetAction(S, REASSOC_WAIT_RSP, MT2_MLME_DISASSOC_REQ,
			      (STATE_MACHINE_FUNC)
			      InvalidStateWhenDisassociate);
	StateMachineSetAction(S, REASSOC_WAIT_RSP, MT2_PEER_DISASSOC_REQ,
			      (STATE_MACHINE_FUNC) PeerDisassocAction);
	StateMachineSetAction(S, REASSOC_WAIT_RSP, MT2_PEER_REASSOC_RSP,
			      (STATE_MACHINE_FUNC) PeerReassocRspAction);
	/* */
	/* Patch, AP doesn't send Reassociate Rsp frame to Station. */
	/* */
	StateMachineSetAction(S, REASSOC_WAIT_RSP, MT2_PEER_ASSOC_RSP,
			      (STATE_MACHINE_FUNC) PeerReassocRspAction);
	StateMachineSetAction(S, REASSOC_WAIT_RSP, MT2_REASSOC_TIMEOUT,
			      (STATE_MACHINE_FUNC) ReassocTimeoutAction);

	/* fourth column */
	StateMachineSetAction(S, DISASSOC_WAIT_RSP, MT2_MLME_ASSOC_REQ,
			      (STATE_MACHINE_FUNC) InvalidStateWhenAssoc);
	StateMachineSetAction(S, DISASSOC_WAIT_RSP, MT2_MLME_REASSOC_REQ,
			      (STATE_MACHINE_FUNC) InvalidStateWhenReassoc);
	StateMachineSetAction(S, DISASSOC_WAIT_RSP, MT2_MLME_DISASSOC_REQ,
			      (STATE_MACHINE_FUNC)
			      InvalidStateWhenDisassociate);
	StateMachineSetAction(S, DISASSOC_WAIT_RSP, MT2_PEER_DISASSOC_REQ,
			      (STATE_MACHINE_FUNC) PeerDisassocAction);
	StateMachineSetAction(S, DISASSOC_WAIT_RSP, MT2_DISASSOC_TIMEOUT,
			      (STATE_MACHINE_FUNC) DisassocTimeoutAction);

	/* initialize the timer */
	RTMPInitTimer(pAd, &pAd->MlmeAux.AssocTimer,
		      GET_TIMER_FUNCTION(AssocTimeout), pAd, false);
	RTMPInitTimer(pAd, &pAd->MlmeAux.ReassocTimer,
		      GET_TIMER_FUNCTION(ReassocTimeout), pAd, false);
	RTMPInitTimer(pAd, &pAd->MlmeAux.DisassocTimer,
		      GET_TIMER_FUNCTION(DisassocTimeout), pAd, false);
}
