/***************************************************************************
                          rotary.h  -  description
                             -------------------
    begin                : Tue Sep 21 2004
    copyright            : (C) 2004 by Tue Haste Andersen
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

#ifndef ROTARY_H
#define ROTARY_H

#include <qstring.h>

/**
  * Virtual class for handling the PowerMate. This is implemented as a separate thread.
  *
  *@author Tue Haste Andersen
  */

const int kiRotaryFilterMaxLen = 50;

static QString kqRotaryMappingP1Phase =   "Player 1, phase adjustment";
static QString kqRotaryMappingP1Scratch = "Player 1, scratch";
static QString kqRotaryMappingP1Search =  "Player 1, RealSearch";
static QString kqRotaryMappingP2Phase =   "Player 2, phase adjustment";
static QString kqRotaryMappingP2Scratch = "Player 2, scratch";
static QString kqRotaryMappingP2Search =  "Player 2, RealSearch";

class Rotary
{
public:
    Rotary();
    ~Rotary();

    /** Start calibration measurement */
    void calibrateStart();
    /** End calibration measurement */
    double calibrateEnd();
    /** Set calibration */
    void setCalibration(double c);
    /** Low pass filtered rotary event */
    double filter(double dValue);
    /** Collect callibration data */
    void calibrate(double dValue);
    /** Set filter length */
    void setFilterLength(int i);
protected:
    /** Length of filter */
    int m_iFilterLength;
    /** Update position in filter */
    int m_iFilterPos;
    /** Pointer to rotary filter buffer */
    double *m_pFilter;
    /** Calibration value */
    double m_dCalibration;
    /** Last value */
    double m_dLastValue;
    int m_iCalibrationCount;
};

#endif
