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

#include <stdarg.h>
#include <sys/time.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>

#include <TraceLog.h>

/* constants */

#define MAX_STRING_SIZE 256

/*
 * coding convention is leading underscore for global data,
 * trailing underscore for function arguments, and no leading
 * or trailing underscores for local stack variables
 */

/*************************
 * private "member" data
 *************************/

static TraceLogFunction _logFunction = NULL;
static char _logPrefix[MAX_STRING_SIZE] = {"TRACE"};
static pthread_mutex_t _mutex = PTHREAD_MUTEX_INITIALIZER;

/***************************************
 * private "member" function prototypes
 ***************************************/
 
static void printLine(char *line_);
static void formatHeader(const char *type_,
                         const char *file_,
                         const char *function_,
                         int line_,
                         char *outputString_);

/**************************************
 * public API "member" function bodies
 **************************************/

/******************************************************************************/
/******************************************************************************/
void trace_registerLogFunction(TraceLogFunction logFunction_)
{
  if (logFunction_ != NULL)
  {
    _logFunction = logFunction_;
  }
  else
  {
    TRACE_ERROR("NULL logFunction, not registered");
  }
}

/******************************************************************************/
/******************************************************************************/
void trace_setLogPrefix(const char *name_)
{
  if ((name_ != NULL) && (strlen(name_) > 0))
  {
    int length = strlen(name_);
    for (int i = 0; i < length; i++)
    {
      _logPrefix[i] = toupper(name_[i]);
    }
    _logPrefix[length] = '_';
    _logPrefix[length+1] = 0;
  }
  else
  {
    _logPrefix[0] = 0;
  }
}

/******************************************************************************/
/******************************************************************************/
void trace_outputLog(const char *type_,
                     const char *file_,
                     const char *function_,
                     int line_,
                     const char *format_, ...)
{
  pthread_mutex_lock(&_mutex);
  char outputString[MAX_STRING_SIZE];
  formatHeader(type_, file_, function_, line_, outputString);
  va_list args;
  va_start(args, format_);
  vsprintf(&outputString[strlen(outputString)], format_, args);
  va_end(args);
  strcat(outputString, "\n");
  printLine(outputString);
  pthread_mutex_unlock(&_mutex);
}

/******************************************************************************/
/******************************************************************************/
void trace_outputDump(void *address_,
                      unsigned length_,
                      const char *type_,
                      const char *file_,
                      const char *function_,
                      int line_,
                      const char *format_, ...)
{
  pthread_mutex_lock(&_mutex);
  char asciiLine[80];
  const unsigned bytesPerLine = 16;
  unsigned char *bytes = (unsigned char *)address_;
  unsigned short offset = 0;
  char outputString[MAX_STRING_SIZE];
  formatHeader(type_, file_, function_, line_, outputString);
  va_list args;
  va_start(args, format_);
  vsprintf(&outputString[strlen(outputString)], format_, args);
  va_end(args);
  strcat(outputString, "\n");
  printLine(outputString);
  asciiLine[0] = 0;
  for (unsigned i = 0; i < length_; i++)
  {
    // see if we are on a full line boundry
    if ((i%bytesPerLine) == 0)
    {
      // see if we need to print our ascii data line
      if (i > 0)
      {
        sprintf(outputString, "  %s\n", asciiLine);
        printLine(outputString);
        // asciiLine printed, clear it for next time
        asciiLine[0] = 0;
      }
      // print our offset
      sprintf(outputString, "  %04x  ", offset);
      printLine(outputString);
      offset += bytesPerLine;
    }
    // create our line of ascii data, for non-printable ascii characters just use a "."
    sprintf(&asciiLine[strlen(asciiLine)], "%c", ((isprint(bytes[i]) && (bytes[i] < 128)) ? bytes[i] : '.'));
    // print one hex byte
    sprintf(outputString, "%02x ", bytes[i]);
    printLine(outputString);
  }
  // done, print the final ascii line
  sprintf(outputString, "  %*s\n", (int)(((bytesPerLine-strlen(asciiLine))*3)+strlen(asciiLine)), asciiLine);
  printLine(outputString);
  pthread_mutex_unlock(&_mutex);
}

/**************************************
 * private "member" function bodies
 **************************************/

/******************************************************************************/
/******************************************************************************/
void formatHeader(const char *type_, const char *file_, const char *function_, int line_, char *outputString_)
{
  char timestamp[MAX_STRING_SIZE];
  struct timeval tv;
  struct tm tm;
  const char *file;
  // get timestamp
  gettimeofday(&tv, NULL);
  gmtime_r(&tv.tv_sec, &tm);
  // timestamp with date & time
  //strftime(timestamp, sizeof(timestamp), "%m/%d/%y %T", &tm);
  // timestamp with time only
  strftime(timestamp, sizeof(timestamp), "%T", &tm);
  // strip off any leading path of filename
  if ((file = strrchr(file_, '/')) != NULL)
  {
    file++;
  }
  else
  {
    file = file_;
  }
  sprintf(outputString_, "%s%-7s | %s.%ld | %s(%s):%d | ", _logPrefix, type_, timestamp, tv.tv_usec, file, function_, line_);
}

/******************************************************************************/
/******************************************************************************/
void printLine(char *line_)
{
  if (_logFunction == NULL)
  {
    printf("%s", line_);
  }
  else
  {
    (*_logFunction)(line_);
  }
}
