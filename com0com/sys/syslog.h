/*
 * $Id$
 *
 * Copyright (c) 2004-2011 Vyacheslav Frolov
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
 * Revision 1.1  2005/01/26 12:18:54  vfrolov
 * Initial revision
 *
 *
 */

#ifndef _C0C_SYSLOG_H_
#define _C0C_SYSLOG_H_

VOID SysLogDrv(
    IN PDRIVER_OBJECT pDrvObj,
    IN NTSTATUS status,
    IN PWCHAR pStr);

VOID SysLogDev(
    IN PDEVICE_OBJECT pDevObj,
    IN NTSTATUS status,
    IN PWCHAR pStr);

#endif /* _C0C_SYSLOG_H_ */
