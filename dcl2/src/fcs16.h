/*
The MIT License (MIT)

Original work Copyright (c) 2015 Bang & Olufsen
Modified work Copyright (c) 2018 FMFI ŠVT / Project Deadlock

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/**
 * @file fcs16.h
 */

#ifndef FCS16_H
#define FCS16_H

/** FCS initialization value. */
#define FCS16_INIT_VALUE 0xFFFF

/** FCS value for valid frames. */
#define FCS16_GOOD_VALUE 0xF0B8

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Calculates a new FCS based on the current value and value of data.
 *
 * @param fcs Current FCS value
 * @param value The value to be added
 * @returns Calculated FCS value
 */
unsigned short fcs16(unsigned short fcs, unsigned char value);

#ifdef __cplusplus
}
#endif

#endif