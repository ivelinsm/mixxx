/***************************************************************************
                          powermate.cpp  -  description
                             -------------------
    begin                : Tue Apr 29 2003
    copyright            : (C) 2003 by Tue & Ken Haste Andersen
    email                : haste@diku.dk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "powermate.h"
#include "rotary.h"
#include "controlobject.h"
#include "controleventmidi.h"
#include "qapplication.h"
#include "midiobject.h"
#include "mathstuff.h"

PowerMate::PowerMate() : Input()
{
    m_pRequestLed = new QSemaphore(5);
    m_pRotary = new Rotary();
    m_pControlObjectButton = 0;
    m_pControlObjectRotary = 0;
}

PowerMate::~PowerMate()
{
    if (running())
    {
        terminate();
        wait();
    }
    delete m_pRequestLed;
    delete m_pRotary;
}

void PowerMate::led()
{
    m_pRequestLed->tryAccess(1);
}
