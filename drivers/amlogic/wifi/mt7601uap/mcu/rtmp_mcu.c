/*
 *************************************************************************
 * Ralink Tech Inc.
 * 5F., No.36, Taiyuan St., Jhubei City,
 * Hsinchu County 302,
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2002-2010, Ralink Technology, Inc.
 *
 * This program is free software; you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation; either version 2 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program; if not, write to the                         *
 * Free Software Foundation, Inc.,                                       *
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 *                                                                       *
 *************************************************************************/


#include	"rt_config.h"

int MCUBurstWrite(PRTMP_ADAPTER pAd, UINT32 Offset, UINT32 *Data, UINT32 Cnt)
{
	RTUSBMultiWrite_nBytes(pAd, Offset, Data, Cnt * 4, 64); 
}

int MCURandomWrite(PRTMP_ADAPTER pAd, RTMP_REG_PAIR *RegPair, UINT32 Num)
{
	UINT32 Index;
	
	for (Index = 0; Index < Num; Index++)
		RTMP_IO_WRITE32(pAd, RegPair->Register, RegPair->Value);
}

VOID ChipOpsMCUHook(PRTMP_ADAPTER pAd, enum MCU_TYPE MCUType)
{

	RTMP_CHIP_OP *pChipOps = &pAd->chipOps;


	if (MCUType == M8051) 
	{
		pChipOps->sendCommandToMcu = RtmpAsicSendCommandToMcu;
		pChipOps->BurstWrite = MCUBurstWrite;
		pChipOps->RandomWrite = MCURandomWrite;
	}

#ifdef CONFIG_ANDES_SUPPORT
	if (MCUType == ANDES) 
	{

#ifdef RTMP_USB_SUPPORT
		pChipOps->loadFirmware = USBLoadFirmwareToAndes;
#endif
		//pChipOps->sendCommandToMcu = AsicSendCmdToAndes;
		pChipOps->Calibration = AndesCalibrationOP;
		pChipOps->BurstWrite =  AndesBurstWrite;
		pChipOps->BurstRead = AndesBurstRead;
		pChipOps->RandomRead = AndesRandomRead;
		pChipOps->RFRandomRead = AndesRFRandomRead;
		pChipOps->ReadModifyWrite = AndesReadModifyWrite;
		pChipOps->RFReadModifyWrite = AndesRFReadModifyWrite;
		pChipOps->RandomWrite = AndesRandomWrite;
		pChipOps->RFRandomWrite = AndesRFRandomWrite;
		pChipOps->PwrSavingOP = AndesPwrSavingOP;
	}
#endif
}

NDIS_STATUS isMCUNeedToLoadFIrmware(
        IN PRTMP_ADAPTER pAd)
{
        NDIS_STATUS             Status = NDIS_STATUS_SUCCESS;
        ULONG                   Index;
        UINT32                  MacReg;

        Index = 0;

#ifdef CONFIG_ANDES_SUPPORT
	if (WaitForAsicReady(pAd) == TRUE)
	{
		DBGPRINT(RT_DEBUG_TRACE,("%s WaitForAsicReady ====> TRUE\n", __FUNCTION__));	
		MT7601_WLAN_ChipOnOff(pAd, TRUE, TRUE);
	}
	else
	{
		DBGPRINT(RT_DEBUG_ERROR,("%s ====> \n", __FUNCTION__));
	}
#endif
        do {
                if (RTMP_TEST_FLAG(pAd, fRTMP_ADAPTER_NIC_NOT_EXIST))
                        return NDIS_STATUS_FAILURE;

#ifdef CONFIG_ANDES_SUPPORT
                RTMP_IO_READ32(pAd, COM_REG0, &MacReg);
                                
                if (MacReg == 0x1) 
                        break;

                RtmpOsMsDelay(10);
#else
                RTMP_IO_READ32(pAd, PBF_SYS_CTRL, &MacReg);

                if (MacReg & 0x100) /* check bit 8*/
                        break;

                RTMPusecDelay(1000);
#endif
        } while (Index++ < 100);

        if (Index >= 100)
                Status = NDIS_STATUS_FAILURE;
        return Status;
}
