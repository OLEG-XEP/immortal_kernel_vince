/*
 ***************************************************************************
 * Ralink Tech Inc.
 * 4F, No. 2 Technology	5th	Rd.
 * Science-based Industrial	Park
 * Hsin-chu, Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2006, Ralink Technology, Inc.
 *
 * All rights reserved.	Ralink's source	code is	an unpublished work	and	the
 * use of a	copyright notice does not imply	otherwise. This	source code
 * contains	confidential trade secret material of Ralink Tech. Any attemp
 * or participation	in deciphering,	decoding, reverse engineering or in	any
 * way altering	the	source code	is stricitly prohibited, unless	the	prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************

 	Module Name:
	cmm_cmd.c

	Abstract:
	All command related API.

	Revision History:
	Who			When	    What
	--------	----------  ----------------------------------------------
	Name		Date	    Modification logs
	Paul Lin    06-25-2004  created
*/

#include "rt_config.h"

/*
	========================================================================

	Routine Description:

	Arguments:

	Return Value:

	IRQL =

	Note:

	========================================================================
*/
int RTEnqueueInternalCmd(
	IN struct rtmp_adapter *pAd,
	IN NDIS_OID			Oid,
	IN PVOID			pInformationBuffer,
	IN uint32_t 		InformationBufferLength)
{
	int status;
	PCmdQElmt	cmdqelmt = NULL;

#ifdef DBG
	if (RTDebugLevel >= RT_DEBUG_TRACE){
		printk("%s: %s: Oid=%08x, BufLen=%u\n", __FILE__, __func__, Oid, InformationBufferLength);
		if(InformationBufferLength)
			print_hex_dump(KERN_INFO, "", DUMP_PREFIX_OFFSET, 16, InformationBufferLength/16+1, pInformationBuffer, InformationBufferLength, true);
	}
#endif

	cmdqelmt = kzalloc(sizeof(CmdQElmt), GFP_ATOMIC);
	if (cmdqelmt == NULL)
		return (NDIS_STATUS_RESOURCES);

	if(InformationBufferLength > 0)
	{
		cmdqelmt->buffer = kmalloc(InformationBufferLength, GFP_ATOMIC);
		if (cmdqelmt->buffer == NULL) {
			kfree(cmdqelmt);
			return (NDIS_STATUS_RESOURCES);
		}
		else
		{
			memmove(cmdqelmt->buffer, pInformationBuffer, InformationBufferLength);
			cmdqelmt->bufferlength = InformationBufferLength;
		}
	}

	cmdqelmt->command = Oid;

	spin_lock_bh(&pAd->CmdQLock);
	if (pAd->CmdQState & RTMP_TASK_CAN_DO_INSERT)
	{
		list_add_tail(&cmdqelmt->list, &pAd->CmdQ);
		status = NDIS_STATUS_SUCCESS;
	}
	else
	{
		status = NDIS_STATUS_FAILURE;
	}
	spin_unlock_bh(&pAd->CmdQLock);

	if (status == NDIS_STATUS_FAILURE)
	{
		if (cmdqelmt->buffer)
			kfree(cmdqelmt->buffer);
		kfree(cmdqelmt);
	}
	else
		RTCMDUp(&pAd->cmdQTask);

	return(NDIS_STATUS_SUCCESS);
}
