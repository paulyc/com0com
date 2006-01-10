/*
 * $Id$
 *
 * Copyright (c) 2004-2006 Vyacheslav Frolov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 * $Log$
 * Revision 1.22  2005/12/06 13:04:32  vfrolov
 * Fixed data types
 *
 * Revision 1.21  2005/12/05 10:54:55  vfrolov
 * Implemented IOCTL_SERIAL_IMMEDIATE_CHAR
 *
 * Revision 1.20  2005/11/30 16:04:11  vfrolov
 * Implemented IOCTL_SERIAL_GET_STATS and IOCTL_SERIAL_CLEAR_STATS
 *
 * Revision 1.19  2005/11/29 12:33:21  vfrolov
 * Changed SetModemStatus() to ability set and clear bits simultaneously
 *
 * Revision 1.18  2005/11/29 08:35:13  vfrolov
 * Implemented SERIAL_EV_RX80FULL
 *
 * Revision 1.17  2005/11/25 08:59:39  vfrolov
 * Implemented SERIAL_EV_RXFLAG
 *
 * Revision 1.16  2005/09/14 13:14:47  vfrolov
 * Fixed possible tick loss
 *
 * Revision 1.15  2005/09/14 10:42:38  vfrolov
 * Implemented SERIAL_EV_TXEMPTY
 *
 * Revision 1.14  2005/09/13 14:56:16  vfrolov
 * Implemented IRP_MJ_FLUSH_BUFFERS
 *
 * Revision 1.13  2005/09/13 08:55:41  vfrolov
 * Disabled modem status tracing by default
 *
 * Revision 1.12  2005/09/06 07:23:44  vfrolov
 * Implemented overrun emulation
 *
 * Revision 1.11  2005/08/26 08:35:05  vfrolov
 * Fixed unwanted interference to baudrate emulation by read operations
 *
 * Revision 1.10  2005/08/25 15:38:17  vfrolov
 * Some code moved from io.c to bufutils.c
 *
 * Revision 1.9  2005/08/25 08:25:40  vfrolov
 * Fixed data types
 *
 * Revision 1.8  2005/08/23 15:49:21  vfrolov
 * Implemented baudrate emulation
 *
 * Revision 1.7  2005/07/14 12:24:31  vfrolov
 * Replaced ASSERT by HALT_UNLESS
 *
 * Revision 1.6  2005/05/19 08:23:41  vfrolov
 * Fixed data types
 *
 * Revision 1.5  2005/05/14 17:07:02  vfrolov
 * Implemented SERIAL_LSRMST_MST insertion
 *
 * Revision 1.4  2005/05/13 16:58:03  vfrolov
 * Implemented IOCTL_SERIAL_LSRMST_INSERT
 *
 * Revision 1.3  2005/05/13 06:32:16  vfrolov
 * Implemented SERIAL_EV_RXCHAR
 *
 * Revision 1.2  2005/02/01 08:36:27  vfrolov
 * Changed SetModemStatus() to set multiple bits and set CD to DSR
 *
 * Revision 1.1  2005/01/26 12:18:54  vfrolov
 * Initial revision
 *
 */

#include "precomp.h"
#include "timeout.h"
#include "delay.h"
#include "bufutils.h"
#include "handflow.h"

/*
 * FILE_ID used by HALT_UNLESS to put it on BSOD
 */
#define FILE_ID 1

#define GET_REST_BUFFER(pIrp, done) \
    (((PUCHAR)(pIrp)->AssociatedIrp.SystemBuffer) + done)

typedef struct _RW_DATA {

  #define RW_DATA_TYPE_IRP   1
  #define RW_DATA_TYPE_CHR   2

  short type;

  union {
    struct {
      PIRP pIrp;
      NTSTATUS status;
    } irp;
    struct {
      UCHAR chr;
      BOOLEAN isChr;
    } chr;
  } data;
} RW_DATA, *PRW_DATA;

ULONG GetWriteLength(IN PIRP pIrp)
{
  PIO_STACK_LOCATION pIrpStack = IoGetCurrentIrpStackLocation(pIrp);

  switch(pIrpStack->MajorFunction) {
  case IRP_MJ_WRITE:
    return pIrpStack->Parameters.Write.Length;
  case IRP_MJ_DEVICE_CONTROL:
    if (pIrpStack->Parameters.DeviceIoControl.IoControlCode == IOCTL_SERIAL_IMMEDIATE_CHAR)
      return sizeof(UCHAR);
    break;
  }
  return 0;
}

NTSTATUS ReadBuffer(PIRP pIrp, PC0C_BUFFER pBuf, PSIZE_T pReadDone)
{
  NTSTATUS status;
  SIZE_T readLength, information;
  SIZE_T readDone;

  readLength = IoGetCurrentIrpStackLocation(pIrp)->Parameters.Read.Length;
  information = pIrp->IoStatus.Information;

  readDone =  ReadFromBuffer(pBuf, GET_REST_BUFFER(pIrp, information), readLength - information);

  if (readDone) {
    *pReadDone += readDone;
    information += readDone;
    pIrp->IoStatus.Information = information;
  }


  if (information == readLength)
    status = STATUS_SUCCESS;
  else
    status = STATUS_PENDING;

  return status;
}

VOID OnRxChars(
    PC0C_IO_PORT pReadIoPort,
    SIZE_T size,
    PC0C_FLOW_FILTER pFlowFilter,
    PLIST_ENTRY pQueueToComplete)
{
  SetXonXoffHolding(pReadIoPort, pFlowFilter->lastXonXoff);

  if (pFlowFilter->events & (C0C_FLOW_FILTER_EV_RXCHAR | C0C_FLOW_FILTER_EV_RXFLAG)) {
    if (pFlowFilter->events & C0C_FLOW_FILTER_EV_RXCHAR)
      pReadIoPort->eventMask |= SERIAL_EV_RXCHAR;

    if (pFlowFilter->events & C0C_FLOW_FILTER_EV_RXFLAG)
      pReadIoPort->eventMask |= SERIAL_EV_RXFLAG;

    if (pReadIoPort->eventMask)
      WaitComplete(pReadIoPort, pQueueToComplete);
  }

  pReadIoPort->perfStats.ReceivedCount += (ULONG)size;
  pReadIoPort->pDevExt->pIoPortRemote->perfStats.TransmittedCount += (ULONG)size;
}

VOID WriteBuffer(
    PRW_DATA pDataWrite,
    PC0C_IO_PORT pReadIoPort,
    PLIST_ENTRY pQueueToComplete,
    PSIZE_T pWriteLimit,
    PSIZE_T pWriteDone)
{
  SIZE_T writeLength, information;
  SIZE_T writeDone;
  C0C_FLOW_FILTER flowFilter;
  PVOID pWriteBuf;
  PC0C_BUFFER pBuf;
  SIZE_T length;

  if (pDataWrite->type == RW_DATA_TYPE_IRP) {
    PIRP pIrp = pDataWrite->data.irp.pIrp;

    information = pIrp->IoStatus.Information;
    pWriteBuf = GET_REST_BUFFER(pIrp, information);
    writeLength = GetWriteLength(pIrp);
  } else {
    HALT_UNLESS1(pDataWrite->type == RW_DATA_TYPE_CHR, pDataWrite->type);

    information = 0;
    pWriteBuf = &pDataWrite->data.chr.chr;
    writeLength = pDataWrite->data.chr.isChr ? 1 : 0;
  }

  pBuf = &pReadIoPort->readBuf;
  length = writeLength - information;

  if (pWriteLimit && length > *pWriteLimit)
    length = *pWriteLimit;

  FlowFilterInit(pReadIoPort, &flowFilter);

  writeDone = WriteToBuffer(pBuf, pWriteBuf, length, &flowFilter);

  if (writeDone) {
    *pWriteDone += writeDone;
    information += writeDone;

    if (pDataWrite->type == RW_DATA_TYPE_IRP) {
      pDataWrite->data.irp.pIrp->IoStatus.Information = information;
      pReadIoPort->pDevExt->pIoPortRemote->amountInWriteQueue -= (ULONG)writeDone;
    }

    if (pWriteLimit)
      *pWriteLimit -= writeDone;

    OnRxChars(pReadIoPort, writeDone, &flowFilter, pQueueToComplete);
  }

  if (information == writeLength) {
    if (pDataWrite->type == RW_DATA_TYPE_IRP) {
      pDataWrite->data.irp.status = STATUS_SUCCESS;
    } else {
      HALT_UNLESS1(pDataWrite->type == RW_DATA_TYPE_CHR, pDataWrite->type);

      pDataWrite->data.chr.isChr = FALSE;
    }
  }
}

VOID AlertOverrun(PC0C_IO_PORT pReadIoPort, PLIST_ENTRY pQueueToComplete)
{
  pReadIoPort->errors |= SERIAL_ERROR_QUEUEOVERRUN;

  if (pReadIoPort->pDevExt->handFlow.FlowReplace & SERIAL_ERROR_CHAR)
    WriteMandatoryToBuffer(&pReadIoPort->readBuf, pReadIoPort->pDevExt->specialChars.ErrorChar);

  if (pReadIoPort->pDevExt->handFlow.ControlHandShake & SERIAL_ERROR_ABORT) {
    CancelQueue(&pReadIoPort->irpQueues[C0C_QUEUE_READ], pQueueToComplete);
    CancelQueue(&pReadIoPort->irpQueues[C0C_QUEUE_WRITE], pQueueToComplete);
  }
}

VOID WriteOverrun(
    PRW_DATA pDataWrite,
    PC0C_IO_PORT pReadIoPort,
    PLIST_ENTRY pQueueToComplete,
    PSIZE_T pWriteLimit,
    PSIZE_T pWriteDone)
{
  SIZE_T writeLength, information;
  SIZE_T writeDone, readDone;
  C0C_FLOW_FILTER flowFilter;
  PVOID pWriteBuf;
  SIZE_T length;

  if (pDataWrite->type == RW_DATA_TYPE_IRP) {
    PIRP pIrp = pDataWrite->data.irp.pIrp;

    information = pIrp->IoStatus.Information;
    pWriteBuf = GET_REST_BUFFER(pIrp, information);
    writeLength = GetWriteLength(pIrp);
  } else {
    HALT_UNLESS1(pDataWrite->type == RW_DATA_TYPE_CHR, pDataWrite->type);

    information = 0;
    pWriteBuf = &pDataWrite->data.chr.chr;
    writeLength = pDataWrite->data.chr.isChr ? 1 : 0;
  }

  length = writeLength - information;

  if (pWriteLimit && length > *pWriteLimit)
    length = *pWriteLimit;

  FlowFilterInit(pReadIoPort, &flowFilter);

  CopyCharsWithEscape(
      &pReadIoPort->readBuf, &flowFilter,
      NULL, 0,
      pWriteBuf, length,
      &readDone, &writeDone);

  if (writeDone) {
    *pWriteDone += writeDone;
    information += writeDone;

    if (pDataWrite->type == RW_DATA_TYPE_IRP) {
      pDataWrite->data.irp.pIrp->IoStatus.Information = information;
      pReadIoPort->pDevExt->pIoPortRemote->amountInWriteQueue -= (ULONG)writeDone;
    }

    if (pWriteLimit)
      *pWriteLimit -= writeDone;

    if (readDone) {
      AlertOverrun(pReadIoPort, pQueueToComplete);
      pReadIoPort->perfStats.BufferOverrunErrorCount += (ULONG)readDone;
    }

    OnRxChars(pReadIoPort, writeDone, &flowFilter, pQueueToComplete);
  }

  if (information == writeLength) {
    if (pDataWrite->type == RW_DATA_TYPE_IRP) {
      pDataWrite->data.irp.status = STATUS_SUCCESS;
    } else {
      HALT_UNLESS1(pDataWrite->type == RW_DATA_TYPE_CHR, pDataWrite->type);

      pDataWrite->data.chr.isChr = FALSE;
    }
  }
}

VOID ReadWriteDirect(
    PIRP pIrpRead,
    PRW_DATA pDataWrite,
    PNTSTATUS pStatusRead,
    PC0C_IO_PORT pReadIoPort,
    PLIST_ENTRY pQueueToComplete,
    PSIZE_T pWriteLimit,
    PSIZE_T pReadDone,
    PSIZE_T pWriteDone)
{
  SIZE_T readDone, writeDone;
  SIZE_T writeLength, readLength;
  C0C_FLOW_FILTER flowFilter;
  PVOID pWriteBuf, pReadBuf;

  pReadBuf = GET_REST_BUFFER(pIrpRead, pIrpRead->IoStatus.Information);
  readLength = IoGetCurrentIrpStackLocation(pIrpRead)->Parameters.Read.Length
                                                - pIrpRead->IoStatus.Information;

  if (pDataWrite->type == RW_DATA_TYPE_IRP) {
    PIRP pIrpWrite = pDataWrite->data.irp.pIrp;

    pWriteBuf = GET_REST_BUFFER(pIrpWrite, pIrpWrite->IoStatus.Information);
    writeLength = GetWriteLength(pIrpWrite) - pIrpWrite->IoStatus.Information;
  } else {
    HALT_UNLESS1(pDataWrite->type == RW_DATA_TYPE_CHR, pDataWrite->type);

    pWriteBuf = &pDataWrite->data.chr.chr;
    writeLength = pDataWrite->data.chr.isChr ? 1 : 0;
  }

  FlowFilterInit(pReadIoPort, &flowFilter);

  CopyCharsWithEscape(
      &pReadIoPort->readBuf, &flowFilter,
      pReadBuf, readLength,
      pWriteBuf, (pWriteLimit && writeLength > *pWriteLimit) ? *pWriteLimit : writeLength,
      &readDone, &writeDone);

  if (pWriteLimit)
    *pWriteLimit -= writeDone;

  pIrpRead->IoStatus.Information += readDone;

  if (pDataWrite->type == RW_DATA_TYPE_IRP) {
    pDataWrite->data.irp.pIrp->IoStatus.Information += writeDone;
    pReadIoPort->pDevExt->pIoPortRemote->amountInWriteQueue -= (ULONG)writeDone;
  }

  if (readDone == readLength)
    *pStatusRead = STATUS_SUCCESS;

  if (writeDone == writeLength) {
    if (pDataWrite->type == RW_DATA_TYPE_IRP) {
      pDataWrite->data.irp.status = STATUS_SUCCESS;
    } else {
      HALT_UNLESS1(pDataWrite->type == RW_DATA_TYPE_CHR, pDataWrite->type);

      pDataWrite->data.chr.isChr = FALSE;
    }
  }

  if (writeDone)
    OnRxChars(pReadIoPort, writeDone, &flowFilter, pQueueToComplete);

  *pReadDone += readDone;
  *pWriteDone += writeDone;
}

VOID InsertDirect(
    PC0C_RAW_DATA pRawData,
    PIRP pIrpRead,
    PNTSTATUS pStatusWrite,
    PNTSTATUS pStatusRead,
    PSIZE_T pReadDone)
{
  SIZE_T readDone;
  SIZE_T readLength;
  PVOID pReadBuf;

  pReadBuf = GET_REST_BUFFER(pIrpRead, pIrpRead->IoStatus.Information);
  readLength = IoGetCurrentIrpStackLocation(pIrpRead)->Parameters.Read.Length
                                                - pIrpRead->IoStatus.Information;

  readDone = WriteRawData(pRawData, pStatusWrite, pReadBuf, readLength);

  pIrpRead->IoStatus.Information += readDone;

  if (readDone == readLength)
    *pStatusRead = STATUS_SUCCESS;

  *pReadDone += readDone;
}

PIRP StartCurrentIrp(PC0C_IRP_QUEUE pQueue, PDRIVER_CANCEL *ppCancelRoutine, PBOOLEAN pFirst)
{
  while (pQueue->pCurrent) {
    PIRP pIrp;

    pIrp = pQueue->pCurrent;

    #pragma warning(push, 3)
    *ppCancelRoutine = IoSetCancelRoutine(pIrp, NULL);
    #pragma warning(pop)

    if (*ppCancelRoutine)
      return pIrp;

    ShiftQueue(pQueue);
    *pFirst = FALSE;
  }
  return NULL;
}

NTSTATUS StopCurrentIrp(
    NTSTATUS status,
    PDRIVER_CANCEL pCancelRoutine,
    BOOLEAN first,
    SIZE_T done,
    PC0C_IO_PORT pIoPort,
    PC0C_IRP_QUEUE pQueue,
    PLIST_ENTRY pQueueToComplete)
{
  PIRP pIrp;

  pIrp = pQueue->pCurrent;

  if (status == STATUS_PENDING && done) {
    PC0C_IRP_STATE pState;

    pState = GetIrpState(pIrp);
    HALT_UNLESS(pState);

    if ((pState->flags & C0C_IRP_FLAG_WAIT_ONE) != 0) {
      status = STATUS_SUCCESS;
    }
    else
    if (first && (pState->flags & C0C_IRP_FLAG_INTERVAL_TIMEOUT) != 0) {
      SetIntervalTimeout(pIoPort);
    }
  }

  if (!first && status == STATUS_PENDING)
    status = FdoPortSetIrpTimeout(pIoPort->pDevExt, pIrp);

  HALT_UNLESS(pCancelRoutine);

  if (status == STATUS_PENDING) {
    #pragma warning(push, 3)
    IoSetCancelRoutine(pIrp, pCancelRoutine);
    #pragma warning(pop)
    if (pIrp->Cancel) {
      #pragma warning(push, 3)
      pCancelRoutine = IoSetCancelRoutine(pIrp, NULL);
      #pragma warning(pop)

      if (pCancelRoutine) {
        ShiftQueue(pQueue);
        pIrp->IoStatus.Status = STATUS_CANCELLED;
        InsertTailList(pQueueToComplete, &pIrp->Tail.Overlay.ListEntry);
        return STATUS_CANCELLED;
      }
    }
  } else {
    ShiftQueue(pQueue);
    pIrp->IoStatus.Status = status;
    InsertTailList(pQueueToComplete, &pIrp->Tail.Overlay.ListEntry);
  }

  return status;
}

NTSTATUS FdoPortIo(
    short ioType,
    PVOID pParam,
    PC0C_IO_PORT pIoPort,
    PC0C_IRP_QUEUE pQueue,
    PLIST_ENTRY pQueueToComplete)
{
  NTSTATUS status;
  BOOLEAN first;
  BOOLEAN firstCurrent;
  PIRP pIrpCurrent;
  PDRIVER_CANCEL pCancelRoutineCurrent;
  SIZE_T done;

  first = TRUE;
  done = 0;

  status = STATUS_PENDING;

  for (firstCurrent = TRUE ; (pIrpCurrent = StartCurrentIrp(pQueue, &pCancelRoutineCurrent, &firstCurrent)) != NULL ; firstCurrent = FALSE) {
    NTSTATUS statusCurrent;
    SIZE_T doneCurrent;

    statusCurrent = STATUS_PENDING;
    doneCurrent = 0;

    switch (ioType) {
    case C0C_IO_TYPE_WAIT_COMPLETE:
      HALT_UNLESS(pParam);
      *((PULONG)pIrpCurrent->AssociatedIrp.SystemBuffer) = *((PULONG)pParam);
      *((PULONG)pParam) = 0;
      pIrpCurrent->IoStatus.Information = sizeof(ULONG);
      statusCurrent = STATUS_SUCCESS;
      break;
    case C0C_IO_TYPE_INSERT:
      HALT_UNLESS(pParam);
      InsertDirect((PC0C_RAW_DATA)pParam, pIrpCurrent, &status, &statusCurrent, &doneCurrent);
      break;
    }

    statusCurrent = StopCurrentIrp(statusCurrent,
                                   pCancelRoutineCurrent,
                                   firstCurrent,
                                   doneCurrent,
                                   pIoPort,
                                   pQueue,
                                   pQueueToComplete);

    if (statusCurrent == STATUS_PENDING)
      break;
  }

  if (status == STATUS_PENDING) {
    switch (ioType) {
    case C0C_IO_TYPE_INSERT:
      HALT_UNLESS(pParam);
      status = WriteRawDataToBuffer((PC0C_RAW_DATA)pParam, &pIoPort->readBuf);
      if (status == STATUS_PENDING && !pIoPort->emuOverrun)
        status = MoveRawData(&pIoPort->readBuf.insertData, (PC0C_RAW_DATA)pParam);
      UpdateHandFlow(pIoPort->pDevExt, FALSE, pQueueToComplete);
      break;
    }
  }
  return status;
}

NTSTATUS TryReadWrite(
    PC0C_IO_PORT pIoPortRead,
    BOOLEAN startRead,
    PC0C_IO_PORT pIoPortWrite,
    BOOLEAN startWrite,
    PLIST_ENTRY pQueueToComplete)
{
  NTSTATUS status;
  SIZE_T readBufBusyBeg, readBufBusyEnd;

  RW_DATA dataIrpRead;
  PC0C_IRP_QUEUE pQueueRead;
  BOOLEAN firstRead;
  PDRIVER_CANCEL pCancelRoutineRead;
  SIZE_T doneRead;

  RW_DATA dataCharX;

  RW_DATA dataIrpWrite;
  PC0C_IRP_QUEUE pQueueWrite;
  BOOLEAN firstWrite;
  PDRIVER_CANCEL pCancelRoutineWrite;
  SIZE_T doneWrite;
  BOOLEAN wasWrite;

  PC0C_ADAPTIVE_DELAY pWriteDelay;
  SIZE_T writeLimit;
  PSIZE_T pWriteLimit;

  dataIrpRead.type = RW_DATA_TYPE_IRP;
  dataCharX.type = RW_DATA_TYPE_CHR;
  dataIrpWrite.type = RW_DATA_TYPE_IRP;

  pQueueRead = &pIoPortRead->irpQueues[C0C_QUEUE_READ];
  pQueueWrite = &pIoPortWrite->irpQueues[C0C_QUEUE_WRITE];
  pWriteDelay = pIoPortWrite->pWriteDelay;

  if (pWriteDelay) {
    if (pQueueWrite->pCurrent || pIoPortWrite->sendXonXoff) {
      StartWriteDelayTimer(pWriteDelay);
      writeLimit = GetWriteLimit(pWriteDelay);
      status = STATUS_PENDING;
    } else {
      writeLimit = 0;
      status = STATUS_SUCCESS;
    }

    pWriteLimit = &writeLimit;
  } else {
    status = STATUS_SUCCESS;
    pWriteLimit = NULL;
  }

  readBufBusyBeg = C0C_BUFFER_BUSY(&pIoPortRead->readBuf);

  /* get first pIrpRead */

  dataIrpRead.data.irp.status = STATUS_SUCCESS;
  doneRead = 0;
  firstRead = TRUE;

  if (startRead) {
    dataIrpRead.data.irp.pIrp = pQueueRead->pCurrent;
    pCancelRoutineRead = NULL;
  } else {
    dataIrpRead.data.irp.pIrp = StartCurrentIrp(pQueueRead, &pCancelRoutineRead, &firstRead);
  }

  /* read from buffer */

  while (dataIrpRead.data.irp.pIrp) {
    dataIrpRead.data.irp.status = ReadBuffer(dataIrpRead.data.irp.pIrp, &pIoPortRead->readBuf, &doneRead);

    if (dataIrpRead.data.irp.status == STATUS_PENDING)
      break;

    if (startRead && firstRead) {
      status = dataIrpRead.data.irp.status;
      ShiftQueue(pQueueRead);
    } else {
      StopCurrentIrp(dataIrpRead.data.irp.status, pCancelRoutineRead, firstRead,
                     doneRead, pIoPortRead, pQueueRead, pQueueToComplete);
    }

    /* get next pIrpRead */

    doneRead = 0;
    firstRead = FALSE;

    dataIrpRead.data.irp.pIrp =
        StartCurrentIrp(pQueueRead, &pCancelRoutineRead, &firstRead);
  }

  readBufBusyEnd = C0C_BUFFER_BUSY(&pIoPortRead->readBuf);

  if (readBufBusyBeg > readBufBusyEnd) {
    UpdateHandFlow(pIoPortRead->pDevExt, TRUE, pQueueToComplete);
    readBufBusyBeg = readBufBusyEnd;
  }

  /* get XON or XOFF char */

  switch (pIoPortWrite->sendXonXoff) {
  case C0C_XCHAR_ON:
    dataCharX.data.chr.chr = pIoPortWrite->pDevExt->specialChars.XonChar;
    dataCharX.data.chr.isChr = TRUE;
    break;
  case C0C_XCHAR_OFF:
    dataCharX.data.chr.chr = pIoPortWrite->pDevExt->specialChars.XoffChar;
    dataCharX.data.chr.isChr = TRUE;
    break;
  default:
    dataCharX.data.chr.isChr = FALSE;
  }

  /* get first pIrpWrite */

  wasWrite = FALSE;
  doneWrite = 0;
  firstWrite = TRUE;

  if(startWrite) {
    dataIrpWrite.data.irp.pIrp = pQueueWrite->pCurrent;
    pCancelRoutineWrite = NULL;
  } else {
    dataIrpWrite.data.irp.pIrp =
        StartCurrentIrp(pQueueWrite, &pCancelRoutineWrite, &firstWrite);
  }

  /* read/write direct */

  while (dataIrpRead.data.irp.pIrp) {
    if (dataCharX.data.chr.isChr) {
      if (!pWriteLimit || *pWriteLimit) {
        if ((pIoPortWrite->writeHolding & ~SERIAL_TX_WAITING_FOR_XON) == 0) {
          if (dataIrpRead.data.irp.status == STATUS_PENDING) {
            SIZE_T done = 0;

            ReadWriteDirect(dataIrpRead.data.irp.pIrp,
                            &dataCharX,
                            &dataIrpRead.data.irp.status,
                            pIoPortRead,
                            pQueueToComplete,
                            pWriteLimit,
                            &doneRead, &done);

            if (done) {
              if (pWriteDelay)
                pWriteDelay->sentFrames += done;
            }
          }
        }
        else
        if (pWriteDelay) {
          pWriteDelay->sentFrames += *pWriteLimit;
          *pWriteLimit = 0;
        }
      }
    }

    while (dataIrpWrite.data.irp.pIrp) {
      if (IoGetCurrentIrpStackLocation(dataIrpWrite.data.irp.pIrp)->MajorFunction ==
                                                                IRP_MJ_FLUSH_BUFFERS)
      {
        dataIrpWrite.data.irp.status = STATUS_SUCCESS;
      } else {
        dataIrpWrite.data.irp.status = STATUS_PENDING;

        if (!pWriteLimit || *pWriteLimit) {
          if (!pIoPortWrite->writeHolding) {
            if (dataIrpRead.data.irp.status == STATUS_PENDING) {
              SIZE_T done = 0;

              ReadWriteDirect(dataIrpRead.data.irp.pIrp,
                              &dataIrpWrite,
                              &dataIrpRead.data.irp.status,
                              pIoPortRead,
                              pQueueToComplete,
                              pWriteLimit,
                              &doneRead, &done);

              if (done) {
                doneWrite += done;
                wasWrite = TRUE;

                if (pWriteDelay)
                  pWriteDelay->sentFrames += done;
              }
            }
          }
          else
          if (pWriteDelay) {
            pWriteDelay->sentFrames += *pWriteLimit;
            *pWriteLimit = 0;
          }
        }
      }

      if (dataIrpWrite.data.irp.status == STATUS_PENDING)
        break;

      if(startWrite && firstWrite) {
        status = dataIrpWrite.data.irp.status;
        ShiftQueue(pQueueWrite);
      } else {
        StopCurrentIrp(dataIrpWrite.data.irp.status, pCancelRoutineWrite, firstWrite,
                       doneWrite, pIoPortWrite, pQueueWrite, pQueueToComplete);
      }

      /* get next pIrpWrite */

      doneWrite = 0;
      firstWrite = FALSE;

      dataIrpWrite.data.irp.pIrp =
          StartCurrentIrp(pQueueWrite, &pCancelRoutineWrite, &firstWrite);
    }

    if (startRead && firstRead) {
      if (dataIrpRead.data.irp.status == STATUS_PENDING)
        dataIrpRead.data.irp.status =
            FdoPortSetIrpTimeout(pIoPortRead->pDevExt, dataIrpRead.data.irp.pIrp);

      status = dataIrpRead.data.irp.status;

      if (dataIrpRead.data.irp.status != STATUS_PENDING)
        ShiftQueue(pQueueRead);
    } else {
      dataIrpRead.data.irp.status = StopCurrentIrp(
          dataIrpRead.data.irp.status, pCancelRoutineRead, firstRead,
          doneRead, pIoPortRead, pQueueRead, pQueueToComplete);
    }

    /* get next pIrpRead */

    if (dataIrpRead.data.irp.status != STATUS_PENDING) {
      doneRead = 0;
      firstRead = FALSE;
      dataIrpRead.data.irp.pIrp =
          StartCurrentIrp(pQueueRead, &pCancelRoutineRead, &firstRead);
    } else {
      dataIrpRead.data.irp.pIrp = NULL;
    }
  }

  /* write to buffer */

  if (dataCharX.data.chr.isChr) {
    if (!pWriteLimit || *pWriteLimit) {
      if ((pIoPortWrite->writeHolding & ~SERIAL_TX_WAITING_FOR_XON) == 0) {
        SIZE_T done = 0;

        WriteBuffer(&dataCharX, pIoPortRead,
                    pQueueToComplete, pWriteLimit, &done);

        readBufBusyEnd = C0C_BUFFER_BUSY(&pIoPortRead->readBuf);

        if (readBufBusyBeg < readBufBusyEnd) {
          if ((pIoPortRead->waitMask & SERIAL_EV_RX80FULL) &&
              readBufBusyEnd > pIoPortRead->readBuf.size80 &&
              readBufBusyBeg <= pIoPortRead->readBuf.size80)
          {
            pIoPortRead->eventMask |= SERIAL_EV_RX80FULL;
            WaitComplete(pIoPortRead, pQueueToComplete);
          }

          UpdateHandFlow(pIoPortRead->pDevExt, FALSE, pQueueToComplete);
          readBufBusyBeg = readBufBusyEnd;
        }

        if (pIoPortRead->emuOverrun &&
            dataCharX.data.chr.isChr &&
            (pIoPortWrite->writeHolding & ~SERIAL_TX_WAITING_FOR_XON) == 0 &&
            C0C_BUFFER_BUSY(&pIoPortRead->readBuf) >= C0C_BUFFER_SIZE(&pIoPortRead->readBuf))
        {
          WriteOverrun(&dataCharX, pIoPortRead,
                       pQueueToComplete, pWriteLimit, &done);
        }

        if (done) {
          if (pWriteDelay)
            pWriteDelay->sentFrames += done;
        }
      }
      else
      if (pWriteDelay) {
        pWriteDelay->sentFrames += *pWriteLimit;
        *pWriteLimit = 0;
      }
    }
  }

  if (!dataCharX.data.chr.isChr)
    pIoPortWrite->sendXonXoff = 0;

  while (dataIrpWrite.data.irp.pIrp) {
    if (IoGetCurrentIrpStackLocation(dataIrpWrite.data.irp.pIrp)->MajorFunction ==
                                                              IRP_MJ_FLUSH_BUFFERS)
    {
      dataIrpWrite.data.irp.status = STATUS_SUCCESS;
    } else {
      dataIrpWrite.data.irp.status = STATUS_PENDING;

      if (!pWriteLimit || *pWriteLimit) {
        if (!pIoPortWrite->writeHolding) {
          SIZE_T done = 0;

          WriteBuffer(&dataIrpWrite, pIoPortRead,
                      pQueueToComplete, pWriteLimit, &done);

          readBufBusyEnd = C0C_BUFFER_BUSY(&pIoPortRead->readBuf);

          if (readBufBusyBeg < readBufBusyEnd) {
            if ((pIoPortRead->waitMask & SERIAL_EV_RX80FULL) &&
                readBufBusyEnd > pIoPortRead->readBuf.size80 &&
                readBufBusyBeg <= pIoPortRead->readBuf.size80)
            {
              pIoPortRead->eventMask |= SERIAL_EV_RX80FULL;
              WaitComplete(pIoPortRead, pQueueToComplete);
            }

            UpdateHandFlow(pIoPortRead->pDevExt, FALSE, pQueueToComplete);
            readBufBusyBeg = readBufBusyEnd;
          }

          if (pIoPortRead->emuOverrun &&
              dataIrpWrite.data.irp.status == STATUS_PENDING &&
              !pIoPortWrite->writeHolding &&
              C0C_BUFFER_BUSY(&pIoPortRead->readBuf) >= C0C_BUFFER_SIZE(&pIoPortRead->readBuf))
          {
            WriteOverrun(&dataIrpWrite, pIoPortRead,
                         pQueueToComplete, pWriteLimit, &done);
          }

          if (done) {
            doneWrite += done;
            wasWrite = TRUE;

            if (pWriteDelay)
              pWriteDelay->sentFrames += done;
          }
        }
        else
        if (pWriteDelay) {
          pWriteDelay->sentFrames += *pWriteLimit;
          *pWriteLimit = 0;
        }
      }
    }

    if(startWrite && firstWrite) {
      if (dataIrpWrite.data.irp.status == STATUS_PENDING)
        dataIrpWrite.data.irp.status =
            FdoPortSetIrpTimeout(pIoPortWrite->pDevExt, dataIrpWrite.data.irp.pIrp);

      status = dataIrpWrite.data.irp.status;

      if (dataIrpWrite.data.irp.status != STATUS_PENDING)
        ShiftQueue(pQueueWrite);
    } else {
      dataIrpWrite.data.irp.status =
          StopCurrentIrp(dataIrpWrite.data.irp.status, pCancelRoutineWrite, firstWrite,
                         doneWrite, pIoPortWrite, pQueueWrite, pQueueToComplete);
    }

    /* get next pIrpWrite */

    if (dataIrpWrite.data.irp.status != STATUS_PENDING) {
      doneWrite = 0;
      firstWrite = FALSE;
      dataIrpWrite.data.irp.pIrp = StartCurrentIrp(pQueueWrite, &pCancelRoutineWrite, &firstWrite);
    } else {
      dataIrpWrite.data.irp.pIrp = NULL;
    }
  }

  if (wasWrite && !pQueueWrite->pCurrent &&
      pIoPortWrite->waitMask & SERIAL_EV_TXEMPTY)
  {
    pIoPortWrite->eventMask |= SERIAL_EV_TXEMPTY;
    WaitComplete(pIoPortWrite, pQueueToComplete);
  }

  return status;
}

NTSTATUS ReadWrite(
    PC0C_IO_PORT pIoPortRead,
    BOOLEAN startRead,
    PC0C_IO_PORT pIoPortWrite,
    BOOLEAN startWrite,
    PLIST_ENTRY pQueueToComplete)
{
  NTSTATUS status;

  status = TryReadWrite(
      pIoPortRead, startRead,
      pIoPortWrite, startWrite,
      pQueueToComplete);

  pIoPortWrite->tryWrite = FALSE;

  while (pIoPortRead->tryWrite) {
    PC0C_IO_PORT pIoPortTmp;

    pIoPortTmp = pIoPortRead;
    pIoPortRead = pIoPortWrite;
    pIoPortWrite = pIoPortTmp;

    TryReadWrite(
        pIoPortRead, FALSE,
        pIoPortWrite, FALSE,
        pQueueToComplete);

    pIoPortWrite->tryWrite = FALSE;

    if (status == STATUS_PENDING && (startRead || startWrite))
      break;
  }

  return status;
}

VOID SetModemStatus(
    IN PC0C_IO_PORT pIoPort,
    IN ULONG bits,
    IN ULONG mask,
    PLIST_ENTRY pQueueToComplete)
{
  ULONG modemStatusOld;
  ULONG modemStatusChanged;

  modemStatusOld = pIoPort->modemStatus;

  pIoPort->modemStatus |= bits & mask;
  pIoPort->modemStatus &= ~(~bits & mask);

  /* CD = DSR */
  if (pIoPort->modemStatus & C0C_MSB_DSR)
    pIoPort->modemStatus |= C0C_MSB_RLSD;
  else
    pIoPort->modemStatus &= ~C0C_MSB_RLSD;

  modemStatusChanged = modemStatusOld ^ pIoPort->modemStatus;

  if (modemStatusChanged) {
    TraceModemStatus(pIoPort);

    SetModemStatusHolding(pIoPort);

    if (pIoPort->escapeChar) {
      NTSTATUS status;
      C0C_RAW_DATA insertData;

      insertData.size = 3;
      insertData.data[0] = pIoPort->escapeChar;
      insertData.data[1] = SERIAL_LSRMST_MST;
      insertData.data[2] = (UCHAR)(pIoPort->modemStatus | (modemStatusChanged >> 4));

      status = FdoPortIo(
          C0C_IO_TYPE_INSERT,
          &insertData,
          pIoPort,
          &pIoPort->irpQueues[C0C_QUEUE_READ],
          pQueueToComplete);

      if (status == STATUS_PENDING) {
        AlertOverrun(pIoPort, pQueueToComplete);
        Trace0((PC0C_COMMON_EXTENSION)pIoPort->pDevExt, L"WARNING: Lost SERIAL_LSRMST_MST");
      }
    }

    if (modemStatusChanged & C0C_MSB_CTS)
      pIoPort->eventMask |= pIoPort->waitMask & SERIAL_EV_CTS;

    if (modemStatusChanged & C0C_MSB_DSR)
      pIoPort->eventMask |= pIoPort->waitMask & SERIAL_EV_DSR;

    if (modemStatusChanged & C0C_MSB_RING)
      pIoPort->eventMask |= pIoPort->waitMask & SERIAL_EV_RING;

    if (modemStatusChanged & C0C_MSB_RLSD)
      pIoPort->eventMask |= pIoPort->waitMask & SERIAL_EV_RLSD;

    WaitComplete(pIoPort, pQueueToComplete);
  }
}
