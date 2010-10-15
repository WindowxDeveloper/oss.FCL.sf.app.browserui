/****************************************************************************
**
 * Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This file is part of Qt Web Runtime.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#include <e32std.h>

extern TInt _symbian_SetupThreadHeap(TBool aNotFirst, SStdEpocThreadCreateInfo& aInfo);

/*
 * \internal
 *
 * Uses link-time symbol preemption to capture a call from the application
 * startup. On return, there is some kind of heap allocator installed on the
 * thread.
 */
TInt UserHeap::SetupThreadHeap(TBool aNotFirst, SStdEpocThreadCreateInfo& aInfo)
{
    return _symbian_SetupThreadHeap(aNotFirst, aInfo);
}