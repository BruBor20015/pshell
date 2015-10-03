/*******************************************************************************
 *
 * Copyright (c) 2009, Ron Iovine, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Ron Iovine nor the names of its contributors
 *       may be used to endorse or promote products derived from this software
 *       without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Ron Iovine ''AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL Ron Iovine BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 *******************************************************************************/

#ifndef TRACE_LOG_H
#define TRACE_LOG_H

#include <stdio.h>
#include <TraceFilter.h>

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 *
 * This file and API are an example trace logging service to show how to
 * integrate the trace filtering mechanism into an already existing trace
 * logging service that uses the __FILE__, __LINE__,  __FUNCTION__, level
 * paradigm
 *
 *******************************************************************************/

/*
 * these trace levels must be register into the trace filter service via the
 * 'tf_addLevel' function, that function must be called before calling 'tf_init',
 * in this example, all the levels are added in the function 'trace_registerLevels'
 */
#define TL_ERROR      0
#define TL_WARNING    1
#define TL_FAILURE    2
#define TL_INFO       3
#define TL_DEBUG      4
#define TL_ENTER      5
#define TL_EXIT       6
#define TL_DUMP       7
/* start all user defined levels after the MAX */
#define TL_MAX_LEVELS TL_DUMP

/* 
 * the following are some example TRACE macros to illustrate integrating the
 * function 'tf_isFilterPassed' into an existing trace logging system to provide
 * dynamic trace control via an integrated pshell setver, the existing trace
 * logging utility must ustilize the __FILE__, __FUNCTION__, __LINE__ (and
 * optionally a 'level') trace logging paradigm 
 */

/* trace output macros, these are called directly by client code */

/* this trace cannot be disabled, hence no call to the 'tf_isFilterPassed' function is necessary */
#define TRACE_FORCE(format, args...) trace_outputLog("FORCE", __FILE__, __FUNCTION__, __LINE__, format, ## args)

/* these traces must pass through the 'tf_isFilterPassed' function in order to be displayed */
#define TRACE_ERROR(format, args...) __TRACE(TL_ERROR, "ERROR", format, ## args)
#define TRACE_WARNING(format, args...) __TRACE(TL_WARNING, "WARNING", format, ## args)
#define TRACE_FAILURE(format, args...) __TRACE(TL_FAILURE, "FAILURE", format, ## args)
#define TRACE_INFO(format, args...) __TRACE(TL_INFO, "INFO", format, ## args)
#define TRACE_DEBUG(format, args...) __TRACE(TL_DEBUG, "DEBUG", format, ## args)
#define TRACE_ENTER(format, args...) __TRACE(TL_ENTER, "ENTER", format, ## args)
#define TRACE_EXIT(format, args...) __TRACE(TL_EXIT, "EXIT", format, ## args)

/* hex dump trace, must also pass the 'tf_isFilterPassed' criteria */
#define TRACE_DUMP(address, length, format, args...) __DUMP(address, length, TL_DUMP, "DUMP", format, ## args)

/*
 * trace_registerLogFunction:
 *
 * typedef and function to allow a client program to register a logging
 * function for message output logging, if no output function is registered
 * 'printf' will be used to print out the log messages
 */
typedef void (*TraceLogFunction)(const char *outputString_);
void trace_registerLogFunction(TraceLogFunction logFunction_);

/*
 * trace_setLogPrefix:
 *
 * set a log name that is used as a prefix for the log type,
 * if this function is not called, the prefix will default to
 * "TRACE", e.g. TRACE | ERROR..., TRACE | WARNING... etc, if
 * this function is called with a NULL prefix, the trace name
 * type will have no prefix, e.g. just ERROR..., WARNING... etc.
 */
void trace_setLogPrefix(const char *name_);

/*
 * trace_registerLevels:
 *
 * register all our above trace levels with the trace filter, note
 * that all levels MUST be registered before calling 'tf_init'
 */
void trace_registerLevels(void);

/*
 * trace_showLocation:
 *
 * display the file, function, and line information in the trace logs
 */
void trace_showLocation(bool show_);

/*
 * trace_isLocationEnabled:
 *
 * returns if the trace location format is enabled
 */
bool trace_isLocationEnabled(void);

/*
 * trace_showTimestamp:
 *
 * display the timestamp in the trace logs
 */
void trace_showTimestamp(bool show_);

/*
 * trace_isTimestampEnabled:
 *
 * returns if the trace timestamp format is enabled
 */
bool trace_isTimestampEnabled(void);

/*
 * common TRACE and DUMP output functions and macros that all the different
 * levels map to, these macros should NOT be called directly by client code
 */
extern void trace_outputLog(const char *type_, const char *file_, const char *function_, int line_, const char *format_, ...);
extern void trace_outputDump(void *address_, unsigned length_, const char *type_, const char *file_, const char *function_, int line_, const char *format_, ...);

#define __TRACE(level, name, format, args...) if (tf_isFilterPassed(__FILE__, __LINE__, __FUNCTION__, level)) {trace_outputLog(name, __FILE__, __FUNCTION__, __LINE__, format, ## args);}
#define __DUMP(address, length, level, name, format, args...) if (tf_isFilterPassed(__FILE__, __LINE__, __FUNCTION__, level)) {trace_outputDump(address, length, name, __FILE__, __FUNCTION__, __LINE__, format, ## args);}

#ifdef __cplusplus
}
#endif

#endif
