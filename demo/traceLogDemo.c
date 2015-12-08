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

#include <pthread.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <stdlib.h>

#include <TraceLog.h>

/*******************************************************************************
 *
 * This is an example demo program that shows how to use the TraceLog feature as
 * a stand-alone module without any dynamic trace filtering via the TraceFilter
 * and PshellServer modules
 *
 *******************************************************************************/

/* a couple of functions to show function name filtering */

/******************************************************************************/
/******************************************************************************/
void foo(void)
{
  TRACE_ENTER("message 1");
  sleep(1);
  TRACE_EXIT("message 2");
}

/******************************************************************************/
/******************************************************************************/
void bar(void)
{
  TRACE_ENTER("message 1");
  sleep(1);
  TRACE_EXIT("message 2");
}

/******************************************************************************/
/******************************************************************************/
void sampleLogFunction(const char *outputString_)
{
  /*
   * this sample log function is registered with the TraceLog
   * 'trace_registerLogFunction' call to show a client supplied
   * log function, the string passed in is a fully formatted log
   * string, it is up to the registering application to decide
   * what to do with that string, i.e. write to stdout, write to
   * a custom logfile, write to syslog etc
   */
   
   /* write to stdout */
   printf("%s", outputString_);
   
   /* write to syslog */
   syslog(LOG_INFO, "%s", outputString_);
}

/******************************************************************************/
/******************************************************************************/
void showUsage(void)
{
  printf("\n");
  printf("Usage: traceLogDemo <level>\n");
  printf("\n");
  printf("  where:\n");
  printf("    <level>  - The desired log level value, 0-maxLevels\n");
  printf("\n");
}

#define DUMP_BUFFER_SIZE 256

/*
 * create some user defined levels and macros
 */

/* must start user defined levels after TL_MAX_LEVELS */
#define TL_USER_LEVEL1   TL_MAX_LEVELS+1
#define TL_USER_LEVEL2   TL_MAX_LEVELS+2
#define TL_USER_LEVEL3   TL_MAX_LEVELS+3

/* define the string based names of the trace levels */
#define TL_USER_LEVEL1_STRING "UserLevel1"
#define TL_USER_LEVEL2_STRING "UserLevel2"
#define TL_USER_LEVEL3_STRING "UserLevel3"

/* define some program specific trace macros */
#define TRACE_USER_LEVEL1(format, args...) __TRACE(TL_USER_LEVEL1, TL_USER_LEVEL1_STRING, format, ## args)
#define TRACE_USER_LEVEL2(format, args...) __TRACE(TL_USER_LEVEL2, TL_USER_LEVEL2_STRING, format, ## args)
#define TRACE_USER_LEVEL3(format, args...) __TRACE(TL_USER_LEVEL3, TL_USER_LEVEL3_STRING, format, ## args)

/******************************************************************************/
/******************************************************************************/
int main (int argc, char *argv[])
{

#ifndef TRACE_LOG_DISABLED
  char dumpBuffer[DUMP_BUFFER_SIZE];
#endif
  unsigned logLevel = 0;

  /* validate our command line arguments and get desired log level */
  if (argc == 2)
  {
    logLevel = atoi(argv[1]);
  }
  else
  {
    showUsage();
    return (0);
  }
  
#ifndef TRACE_LOG_DISABLED
  /* initialize a sample memory hex dump buffer */
  for (unsigned i = 0; i < DUMP_BUFFER_SIZE; i++)
  {
    dumpBuffer[i] = i;
  }
#endif

  /*
   * register our standard trace levels with the trace log system 
   * so our trace display can be formatted and aligned correctly
   */
   
  trace_registerLevels();

  /*
   * register our program specific trace log levels with the trace log system
   * this must be done after registering our standard levels so we can keep 
   * track of our max level name string length so our trace display can be 
   * formatted and aligned correctly
   * 
   * format of call is "name", level
   */

  trace_addUserLevel(TL_USER_LEVEL1_STRING, TL_USER_LEVEL1);
  trace_addUserLevel(TL_USER_LEVEL2_STRING, TL_USER_LEVEL2);
  trace_addUserLevel(TL_USER_LEVEL3_STRING, TL_USER_LEVEL3);  

  /* set our log level */
  trace_setLogLevel(logLevel);

  /*
   * optionally set a log prefix, if not set, 'TRACE' will be used,
   * if set to 'NULL', no prefix will be used
   */
   
  trace_setLogPrefix("demo");

  /*
   * register a custom client provided log function, this function will
   * take a fully formatted log message string, it is up to the
   * registering application to decide what to do with that string,
   * i.e. write to stdout, write to custom logfile, write to syslog
   * etc, this is optional, if no log function is registered, the
   * trace logging service will just use 'printf' to output the
   * log message
   */
   
  /* open syslog with our program name */ 
  openlog(argv[0], (LOG_CONS | LOG_PID | LOG_NDELAY), LOG_USER);
  
  /* register our log function */
  trace_registerLogFunction(sampleLogFunction);

  /* issue a trace so we can trigger the TRACE_WATCH functionality */
  TRACE_INFO("First trace");

  /* go into an infinite loop issuing some traces so we can demonstrate dynamic trace filtering */
  for (;;)
  {
    TRACE_WARNING("message 1");
    sleep(1);
    TRACE_INFO("message 2");
    sleep(1);
    foo();
    sleep(1);
    bar();
    sleep(1);
    TRACE_DEBUG("message 3");
    sleep(1);
    TRACE_DUMP(dumpBuffer, sizeof(dumpBuffer), "dumping buffer: dumpBuffer");
    sleep(1);
    TRACE_ERROR("message 4");
    sleep(1);
    TRACE_FAILURE("message 5");
    sleep(1);
    TRACE_USER_LEVEL1("message 6");
    sleep(1);
  }
   
  return (0);
   
}
