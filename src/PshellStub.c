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

#include <stdio.h>
#include <unistd.h>

#include <TraceFilter.h>
#include <PshellServer.h>

/*
 * This module honors the complete public API of the PshellServer and
 * TraceFilter modules but with all the underlying functionality stubbed 
 * out, link with this module (or the corresponding "stub" library) to 
 * easily remove all PSHELL functionality from a program without makeing 
 * any changes to actual source code
 */

/* private "member" data */

static PshellTokens _dummyTokens;

/* public "member" functions */
void pshell_setLogLevel(unsigned level_){};
void pshell_registerLogFunction(PshellLogFunction logFunction_){};
const char *pshell_getResultsString(int results_){return ("PSHELL_UNKNOWN_RESULT");};

void pshell_startServer(const char *serverName_,  
                        PshellServerType serverType_,
                        PshellServerMode serverMode_,
                        const char *hostnameOrIpAddr_, 
                        unsigned port_)
{  
  printf("PSHELL_INFO: STUB Server: %s Started\n", serverName_);
  if (serverMode_ == PSHELL_BLOCKING) for (;;) sleep(300);
}

void pshell_addCommand(PshellFunction function_,
                       const char *command_,
                       const char *description_,
                       const char *usage_,
                       unsigned char minArgs_,
                       unsigned char maxArgs_,
                       bool showUsage_){}

void pshell_runCommand(const char *command_, ...){}

void pshell_printf(const char *format_, ...){}
void pshell_flush(void){}
void pshell_wheel(const char *string_){}
void pshell_march(const char *string_){}
bool pshell_isHelp(void){return (true);}
void pshell_showUsage(void){}
PshellTokens *pshell_tokenize(const char *string_, const char *delimeter_){return (&_dummyTokens);}
unsigned pshell_getLength(const char *string_){return (0);}
bool pshell_isEqual(const char *string1_, const char *string2_){return (true);}
bool pshell_isEqualNoCase(const char *string1_, const char *string2_){return (true);}
bool pshell_isSubString(const char *string1_, const char *string2_, unsigned minChars_){return (true);}
bool pshell_isAlpha(const char *string_){return (true);}
bool pshell_isNumeric(const char *string_){return (true);}
bool pshell_isAlphaNumeric(const char *string_){return (true);}
bool pshell_isDec(const char *string_){return (true);}
bool isHex(const char *string_){return (true);}
bool pshell_isFloat(const char *string_){return (true);}
void *pshell_getVoid(const char *string_){return ((void*)0);}
char *pshell_getString(const char *string_){return ((char*)0);}
bool pshell_getBool(const char *string_){return ((bool)0);}
long pshell_getLong(const char *string_){return ((long)0);}
int pshell_getInt(const char *string_){return ((int)0);}
short pshell_getShort(const char *string_){return ((short)0);}
char pshell_getChar(const char *string_){return ((char)0);}
unsigned pshell_getUnsigned(const char *string_){return ((unsigned)0);}
unsigned long pshell_getUnsignedLong(const char *string_){return ((unsigned long)0);}
unsigned short pshell_getUnsignedShort(const char *string_){return ((unsigned short)0);}
unsigned char pshell_getUnsignedChar(const char *string_){return ((unsigned char)0);}
float pshell_getFloat(const char *string_){return ((float)0.0);}
double pshell_getDouble(const char *string_){return ((double)0.0);}

#ifdef TF_FAST_FILENAME_LOOKUP
int TraceSymbols::_numSymbols = 0;
const char *TraceSymbols::_symbolTable[TF_MAX_TRACE_SYMBOLS];
#endif

void tf_init(const char *configFile_){}
void tf_registerThread(const char *threadName_){}
bool tf_isFilterPassed(const char *file_, int line_, const char *function_, unsigned level_){return (true);}
void tf_watch(const char *file_, int line_, const char *function_, const char *symbol_, void *address_, int width_, const char *format_, tf_TraceControl control_){}
void tf_callback(const char *file_, int line_, const char *function_, const char *callbackName_, tf_TraceCallback callbackFunction_, tf_TraceControl control_){}
