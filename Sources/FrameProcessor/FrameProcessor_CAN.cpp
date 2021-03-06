/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * \file      FrameProcessor_CAN.cpp
 * \brief     Source file for CFrameProcessor_CAN class.
 * \author    Ratnadip Choudhury
 * \copyright Copyright (c) 2011, Robert Bosch Engineering and Business Solutions. All rights reserved.
 *
 * Source file for CFrameProcessor_CAN class.
 */

#include "FrameProcessor_stdafx.h"
#include "include/Utils_macro.h"
#include "Datatypes/MsgBufAll_DataTypes.h"

#include "FrameProcessor_CAN.h"
#include "LogObjectCAN.h"


CFrameProcessor_CAN::CFrameProcessor_CAN():m_ouFormatMsgCAN(m_ouRefTimer)
{
    DIL_GetInterface(CAN, (void**)&m_pouDilCanInterface);

    m_sCANProcParams.m_pILog = nullptr;
    m_sCANProcParams.m_pouCANBuffer = nullptr;
    m_bIsDataLogged = FALSE;

    m_sDataCopyThread.m_hActionEvent = m_ouFSEBufCAN.hGetNotifyingEvent();
}

CFrameProcessor_CAN::~CFrameProcessor_CAN()
{
    vEmptyLogObjArray(m_omLogListTmp);
    vEmptyLogObjArray(m_omLogObjectArray);
}

BOOL CFrameProcessor_CAN::InitInstance(void)
{
    BOOL Result = this->CFrameProcessor_Common::InitInstance();

    return Result;
}

int CFrameProcessor_CAN::ExitInstance(void)
{
    int Result = this->CFrameProcessor_Common::ExitInstance();
    m_ouFSEBufCAN.vClearMessageBuffer();

    return Result;
}

CBaseLogObject* CFrameProcessor_CAN::CreateNewLogObj(const CString& omStrVersion)
{
    CLogObjectCAN* pLogObj = nullptr;
    CString strVersion = CString(m_sCANProcParams.m_acVersion);
    if (strVersion.IsEmpty())
    {
        strVersion = omStrVersion;
    }
    pLogObj = new CLogObjectCAN(strVersion);
    return (static_cast<CBaseLogObject*> (pLogObj));
}

void CFrameProcessor_CAN::DeleteLogObj(CBaseLogObject*& pouLogObj)
{
    CLogObjectCAN* pLogObj = static_cast<CLogObjectCAN*> (pouLogObj);
    if (nullptr != pLogObj)
    {
        delete pLogObj;
        pouLogObj = nullptr;
    }
    else
    {
        ASSERT(false);
    }
}

void CFrameProcessor_CAN::CreateTimeModeMapping(SYSTEMTIME& CurrSysTime,
        UINT64& unAbsTime)
{
    if (m_pouDilCanInterface != nullptr)
    {
        LARGE_INTEGER QueryTickCount;
        m_pouDilCanInterface->DILC_GetTimeModeMapping(CurrSysTime, unAbsTime, QueryTickCount);
    }
}

void CFrameProcessor_CAN::vRetrieveDataFromBuffer(void)
{
    static SFORMATTEDDATA_CAN CurrDataCAN = {0, 0, DIR_RX, CAN_CHANNEL_ALL, 0,
        {'\0'}, TYPE_ID_CAN_NONE, TYPE_MSG_CAN_NONE, ERROR_INVALID," x", {'\0'}, {'\0'}, "", "","", "",
        "", "",  "",  "", 0, RGB(0, 0, 0)
    };
    static sTCANDATA CurrMsgCAN;

    while (m_ouFSEBufCAN.GetMsgCount() > 0)
    {
        m_ouFSEBufCAN.ReadFromBuffer(&CurrMsgCAN);

        if (CurrMsgCAN.m_ucDataType != INTR_FLAG)
        {
            // Update network statistics object.
            //m_sFlexProcParams.m_pouNetworkStat->UpdateNetworkStatistics(
            //                CurrFlxMsg.stcDataMsg.dwHeaderInfoFlags);

            if (m_bLogEnabled == TRUE)
            {
                //check for new logging session
                if(m_bResetAbsTime == TRUE)
                {
                    //update msg reset flag
                    m_ouFormatMsgCAN.m_bResetMsgAbsTime = m_bResetAbsTime;
                    m_ouFormatMsgCAN.m_LogSysTime = m_LogSysTime ;
                    m_bResetAbsTime = FALSE;
                }
                // Format current frame in the necessary settings
                m_ouFormatMsgCAN.vFormatCANDataMsg(&CurrMsgCAN, &CurrDataCAN, m_bExprnFlag_Log);

                USHORT ushBlocks = (USHORT) (m_omLogObjectArray.GetSize());
                for (USHORT i = 0; i < ushBlocks; i++)
                {
                    CBaseLogObject* pouLogObjBase = m_omLogObjectArray.GetAt(i);
                    CLogObjectCAN* pouLogObjCon = static_cast<CLogObjectCAN*> (pouLogObjBase);
                    BOOL bIsDataLog = pouLogObjCon->bLogData(CurrDataCAN);

                    if(bIsDataLog == TRUE)
                    {
                        //m_bIsThreadBlocked = FALSE;
                        m_bIsDataLogged = TRUE;
                    }
                }
            }
        }

        // Add this to the client buffer
        if (m_bClientBufferON)
        {
            m_sCANProcParams.m_pouCANBuffer->WriteIntoBuffer(&CurrMsgCAN);
        }
    }
}


/* STARTS FUNCTIONS WHOSE LOGICS ARE IMPLEMENTED IN THIS CLASS  */
// To initialise this module
HRESULT CFrameProcessor_CAN::FPC_DoInitialisation(SCANPROC_PARAMS* psInitParams)
{
    HRESULT hResult = S_FALSE;

    if (psInitParams != nullptr)
    {
        m_sCANProcParams = *psInitParams;
        ASSERT(nullptr != m_sCANProcParams.m_pouCANBuffer);
        ASSERT(nullptr != m_sCANProcParams.m_pILog);

        m_ouFSEBufCAN.vClearMessageBuffer();
        //m_sDataCopyThread.m_hActionEvent = m_ouFSEBufCAN.hGetNotifyingEvent();
        if (this->CFrameProcessor_Common::DoInitialisation() == S_OK)
        {
            if (m_pouDilCanInterface != nullptr)
            {
                if (m_pouDilCanInterface->DILC_ManageMsgBuf(MSGBUF_ADD, m_sCANProcParams.dwClientID, &m_ouFSEBufCAN) != S_OK)
                {
                    ASSERT(false);
                }
                hResult = S_OK;
            }
        }
        else
        {
            ASSERT(false);
        }
    }
    else
    {
        ASSERT(false);
    }

    return hResult;
}

// To modify the filtering scheme of a logging block
HRESULT CFrameProcessor_CAN::FPC_ApplyFilteringScheme(USHORT ushLogBlkID,
        const SFILTERAPPLIED_CAN& sFilterObj)
{
    HRESULT hResult = S_FALSE;
    CLogObjectCAN* pLogObj = nullptr;

    if (bIsEditingON())
    {
        if (m_omLogListTmp.GetSize() > ushLogBlkID)
        {
            CBaseLogObject* pouBaseLogObj = m_omLogListTmp.GetAt(ushLogBlkID);
            pLogObj = static_cast<CLogObjectCAN*> (pouBaseLogObj);
        }
    }
    else
    {
        if (m_omLogObjectArray.GetSize() > ushLogBlkID)
        {
            CBaseLogObject* pouBaseLogObj = m_omLogObjectArray.GetAt(ushLogBlkID);
            pLogObj = static_cast<CLogObjectCAN*> (pouBaseLogObj);
        }
    }

    //update the filters
    if (nullptr != pLogObj)
    {
        pLogObj->SetFilterInfo(sFilterObj);
        hResult = S_OK;
    }
    else
    {
        ASSERT(false);
    }

    return hResult;
}

// Getter for the filtering scheme of a logging block
HRESULT CFrameProcessor_CAN::FPC_GetFilteringScheme(USHORT ushLogBlk,
        SFILTERAPPLIED_CAN& sFilterObj)
{
    HRESULT hResult = S_FALSE;

    CBaseLogObject* pouBaseLogObj = FindLoggingBlock(ushLogBlk);
    CLogObjectCAN* pouLogObj = static_cast<CLogObjectCAN*> (pouBaseLogObj);

    if (nullptr != pouLogObj)
    {
        pouLogObj->GetFilterInfo(sFilterObj);
        hResult = S_OK;
    }
    else
    {
        ASSERT(false);
    }

    return hResult;
}

// To enable/disable updation of the client flexray frame buffer.
HRESULT CFrameProcessor_CAN::FPC_SetClientCANBufON(bool bEnable)
{
    HRESULT hResult = S_FALSE;

    /* There is only one erroneous situation likely to occur: the client buffer
    doesn't exist and the updation of the same is to be enabled. */
    if (bEnable && (nullptr == m_sCANProcParams.m_pouCANBuffer))
    {
        ;
    }
    else
    {
        m_bClientBufferON =bEnable;
        hResult = S_OK;
    }

    return hResult;
}

// To get the flexray buffer of this module
CBaseCANBufFSE* CFrameProcessor_CAN::FPC_GetCANBuffer(void)
{
    return ((CBaseCANBufFSE*) &m_ouFSEBufCAN);
}
/* ENDS FUNCTIONS WHOSE LOGICS ARE IMPLEMENTED IN THIS CLASS  */


// USE COMMON BASE CLASS ALIAS FUNCTIONS: START
/* Call to enable/disable logging for a particular block. Having ushBlk equal
to FOR_ALL, signifies the operation to be performed for all the blocks */
HRESULT CFrameProcessor_CAN::FPC_EnableLoggingBlock(USHORT ushBlk, bool bEnable)
{
    return EnableLoggingBlock(ushBlk, bEnable);
}

// To enable/disable logging
HRESULT CFrameProcessor_CAN::FPC_EnableLogging(bool bEnable)
{
    return EnableLogging(bEnable, CAN);
}

void CFrameProcessor_CAN::FPC_vCloseLogFile()
{
    USHORT ushBlocks = (USHORT) (m_omLogObjectArray.GetSize());

    CBaseLogObject* pouCurrLogObj  = nullptr;
    for (USHORT i = 0; i < ushBlocks; i++)
    {
        pouCurrLogObj = m_omLogObjectArray.GetAt(i);

        if (pouCurrLogObj != nullptr)
        {
            pouCurrLogObj->bStopOnlyLogging();
            //pouCurrLogObj->vCloseLogFile();
        }
    }
}

/* Call to enable/disable logging for a particular block. Having ushBlk equal
to FOR_ALL, signifies the operation to be performed for all the blocks */
HRESULT CFrameProcessor_CAN::FPC_EnableFilter(USHORT ushBlk, bool bEnable)
{
    return EnableFilter(ushBlk, bEnable);
}

// Query function - client flexray buffer updation status (OFF/ON)
bool CFrameProcessor_CAN::FPC_IsClientCANBufON(void)
{
    return IsClientBufferON();
}

// Query function - current logging status (OFF/ON).
bool CFrameProcessor_CAN::FPC_IsLoggingON(void)
{
    return IsLoggingON();
}

bool CFrameProcessor_CAN::FPC_IsDataLogged(void)
{
    return IsDataLogged();
}

bool CFrameProcessor_CAN::FPC_IsThreadBlocked(void)
{
    return IsThreadBlocked();
}

void CFrameProcessor_CAN::FPC_DisableDataLogFlag(void)
{
    DisableDataLogFlag();
}

// Query function - current filtering status
bool CFrameProcessor_CAN::FPC_IsFilterON(void)
{
    return IsFilterON();
}

// To log a string
HRESULT CFrameProcessor_CAN::FPC_LogString(CString& omStr)
{
    return LogString(omStr);
}

// To add a logging block; must be in editing mode
HRESULT CFrameProcessor_CAN::FPC_AddLoggingBlock(const SLOGINFO& sLogObject)
{
    return AddLoggingBlock(sLogObject);
}

// To remove a logging block by its index in the list; editing mode prerequisite
HRESULT CFrameProcessor_CAN::FPC_RemoveLoggingBlock(USHORT ushBlk)
{
    return RemoveLoggingBlock(ushBlk);
}

// Getter for total number of logging blocks
USHORT CFrameProcessor_CAN::FPC_GetLoggingBlockCount(void)
{
    return GetLoggingBlockCount();
}

// To clear the logging block list
HRESULT CFrameProcessor_CAN::FPC_ClearLoggingBlockList(void)
{
    return ClearLoggingBlockList();
}

// Getter for a logging block by specifying its index in the list
HRESULT CFrameProcessor_CAN::FPC_GetLoggingBlock(USHORT ushBlk, SLOGINFO& sLogObject)
{
    return GetLoggingBlock(ushBlk, sLogObject);
}

// Setter for a logging block by specifying its index in the list
HRESULT CFrameProcessor_CAN::FPC_SetLoggingBlock(USHORT ushBlk,
        const SLOGINFO& sLogObject)
{
    return SetLoggingBlock(ushBlk, sLogObject);
}

// To reset or revoke the modifications made
HRESULT CFrameProcessor_CAN::FPC_Reset(void)
{
    return Reset();
}

// To confirm the modifications made
HRESULT CFrameProcessor_CAN::FPC_Confirm(void)
{
    return Confirm();
}

// To start logging block editing session
HRESULT CFrameProcessor_CAN::FPC_StartEditingSession(void)
{
    return StartEditingSession();
}

// To stop logging block editing session
HRESULT CFrameProcessor_CAN::FPC_StopEditingSession(bool bConfirm)
{
    return StopEditingSession(bConfirm);
}

// Getter for the logging configuration data
HRESULT CFrameProcessor_CAN::FPC_GetConfigData(BYTE** ppvConfigData, UINT& unLength)
{
    return GetConfigData(ppvConfigData, unLength);
}

// Getter for the logging configuration data
HRESULT CFrameProcessor_CAN::FPC_GetConfigData(xmlNodePtr pxmlNodePtr)
{
    return GetConfigData(pxmlNodePtr);
}
// Setter for the logging configuration data
HRESULT CFrameProcessor_CAN::FPC_SetConfigData(BYTE* pvDataStream, const CString& omStrVersion)
{
    return SetConfigData(pvDataStream, omStrVersion);
}
HRESULT CFrameProcessor_CAN::FPC_SetConfigData(xmlDocPtr pDoc)
{
    return SetConfigData(pDoc, CAN);
}
// Empty log object
void CFrameProcessor_CAN::vEmptyLogObjArray(CLogObjArray& omLogObjArray)
{
    USHORT ushBlocks = (USHORT) (omLogObjArray.GetSize());

    if (ushBlocks > 0)
    {
        for (USHORT i = 0; i < ushBlocks; i++)
        {
            CBaseLogObject* pouCurrLogObj = omLogObjArray.GetAt(i);
            DeleteLogObj(pouCurrLogObj);
        }
    }
    omLogObjArray.RemoveAll();
}

//Setter for database files associated
HRESULT CFrameProcessor_CAN::FPC_SetDatabaseFiles(const CStringArray& omList)
{
    return SetDatabaseFiles(omList);
}

// To update the channel baud rate info to logger
HRESULT CFrameProcessor_CAN::FPC_SetChannelBaudRateDetails
(SCONTROLLER_DETAILS* controllerDetails,
 int nNumChannels,ETYPE_BUS eBus)
{
    HRESULT hResult = S_OK;
    SetChannelBaudRateDetails(controllerDetails, nNumChannels,eBus);
    return hResult;
}
void CFrameProcessor_CAN::vSetMeasurementFileName()
{
    USHORT ushBlocks = (USHORT) (m_omLogObjectArray.GetSize());
    for (USHORT i = 0; i < ushBlocks; i++)
    {
        CBaseLogObject* pouLogObjBase = m_omLogObjectArray.GetAt(i);
        CLogObjectCAN* pouLogObjCon = static_cast<CLogObjectCAN*> (pouLogObjBase);
        pouLogObjCon->m_sLogInfo.m_sLogAdvStngs.m_nConnectionCount++;
        pouLogObjCon->vSetMeasurementFileName();
    }
}
