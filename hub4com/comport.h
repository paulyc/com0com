/*
 * $Id$
 *
 * Copyright (c) 2006-2007 Vyacheslav Frolov
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
 * Revision 1.2  2007/02/05 09:33:20  vfrolov
 * Implemented internal flow control
 *
 * Revision 1.1  2007/01/23 09:13:10  vfrolov
 * Initial revision
 *
 *
 */

#ifndef _COMPORT_H
#define _COMPORT_H

///////////////////////////////////////////////////////////////
class ComHub;
class ComParams;
class WriteOverlapped;
class ReadOverlapped;
///////////////////////////////////////////////////////////////
class ComPort
{
  public:
    ComPort(ComHub &_hub);

    BOOL Open(const char *pPath, const ComParams &comParams);
    BOOL Start();
    BOOL Write(LPCVOID pData, DWORD len);
    void OnWrite(WriteOverlapped *pOverlapped, DWORD len, DWORD done);
    void OnRead(ReadOverlapped *pOverlapped, LPCVOID pBuf, DWORD done);
    void AddXoff(int count);
    const string &Name() const { return name; }
    HANDLE Handle() const { return handle; }
    void LostReport();

  private:
    BOOL StartRead();

    string name;
    HANDLE handle;
    ComHub &hub;
    int countReadOverlapped;
    int countXoff;
    BOOL filterX;

    DWORD writeQueueLimit;
    DWORD writeQueued;
    DWORD writeLost;
    DWORD writeLostTotal;
};
///////////////////////////////////////////////////////////////

#endif  // _COMPORT_H