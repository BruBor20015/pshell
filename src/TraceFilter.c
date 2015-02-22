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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <assert.h>

#include <PshellServer.h>
#include <TraceLog.h>
#include <TraceFilter.h>

/*
 * the default config dir is where the server will look for the pshell.conf
 * file if it does not find it in the directory specified by the env variable
 * PSHELL_CONFIG_DIR, or the CWD, change this to any other desired location, 
 * the config file can contain settings that override the defined default 
 * configurations for the title, banner, prompt, address, port, type and mode
 * values, this is also the directory the TraceFilter 'init' function looks in
 * for its startup config file in the event that the environment variable of
 * the same name is not found
 */
#ifndef PSHELL_CONFIG_DIR
#define PSHELL_CONFIG_DIR "/etc"
#endif

/* used for the default legacy behavour if the trace filter is disabled */
extern int g_levelThreshold;

/*
 ***********************************
 * "public" (i.e. global) data
 ***********************************
 */
#ifdef TF_FAST_FILENAME_LOOKUP
int TraceSymbols::_numSymbols = 0;
const char *TraceSymbols::_symbolTable[TF_MAX_TRACE_SYMBOLS];
#endif

/*
 ***********************************
 * "private" structure definitions
 ***********************************
 */
#ifndef TF_MAX_TOKENS
#define TF_MAX_TOKENS 32
#endif
struct Tokens
{
  char *tokens[TF_MAX_TOKENS];
  int numTokens;
};

struct LineFilter
{
  int minLine;
  int maxLine;
};

#ifndef TF_MAX_LINE_FILTERS
#define TF_MAX_LINE_FILTERS 50
#endif
struct FileFilter
{
  const char *filename;
  unsigned level;
  int numLineFilters;
  LineFilter lineFilters[TF_MAX_LINE_FILTERS];
};

struct FunctionFilter
{
  char *functionName;
  unsigned level;
};

struct ThreadFilter
{
  const char *threadName;
  pthread_t threadId;
  unsigned level;
};

struct LevelFilter
{
  const char *name;
  unsigned level;
  bool isDefault;
  bool isMaskable;
};

struct RegisteredThread
{
  const char *threadName;
  pthread_t threadId;
};

/***********************************
 * "private" function prototypes
 ***********************************/
 
static void showConfig(void);
static void showUsage(void);
static void configureFilter(int argc, char *argv[]);
static void loadConfigFile(const char *file_, bool interactive_);
static void addLevelFilter(char *name_, unsigned &level_);
static void addFileFilter(const char *file_, bool interactive_);
static void addFunctionFilter(const char *function_, bool interactive_);
static void addThreadFilter(const char *thread_, bool interactive_);
static FileFilter *findFileFilter(const char *file_);
static FunctionFilter *findFunctionFilter(const char *function_);
static ThreadFilter *findThreadFilter(const char *threadName_);
static ThreadFilter *findThreadFilter(pthread_t threadId_);
static void removeLevelFilter(char *name_, unsigned &level_);
static void removeFileFilter(const char *file_);
static void removeFunctionFilter(const char *function_);
static void removeThreadFilter(const char *thread_);
static void removeAllFileFilters(void);
static void removeAllFunctionFilters(void);
static void removeAllThreadFilters(void);
static void tokenize(char *string_, Tokens &tokens_, const char *delimiter_);
static const char *findSymbol(const char *symbol_);
static RegisteredThread *findRegisteredThread(const char *thread_);
static const void showLevels(void);
static const void showThreads(char *thread_);
static bool isLevel(char *string_);
static bool isWatchHit(void);
static void getWatchValue(void);
#ifdef TF_FAST_FILENAME_LOOKUP
static const void showSymbols(char *symbol_);
#endif

/************************************
 * "private" data
 ************************************/

/*
 * coding convention is leading underscore for global data,
 * trailing underscore for function arguments, and no leading
 * or trailing underscores for local stack variables
 */

/* format for level entry is 'name', 'mask', 'isDefault', 'isMaskable',
 * this is just an example of some common trace levels, the contents of 
 * this structure needs to be customized for the specific levels of the
 * given existing trace system
 */
#ifdef TF_NATIVE_DISCRETE_LEVELS
/* existing trace system uses discrete bitmasks, use those values directly for the 'mask' field */
static LevelFilter _levelFilters[] = {{"ERROR",   TL_ERROR,   true,  true},
                                      {"FAILURE", TL_FAILURE, true,  true},
                                      {"WARNING", TL_WARNING, true,  true},
                                      {"INFO",    TL_INFO,    false, true},
                                      {"DEBUG",   TL_DEBUG,   false, true},
                                      {"ENTER",   TL_ENTER,   false, true},
                                      {"EXIT",    TL_EXIT,    false, true},
                                      {"DUMP",    TL_DUMP,    false, true}};
#else
/* existing trace system uses hierarchical level values, map those to corresponding 
   discrete bitmasks in their correct position based on their level value */
static LevelFilter _levelFilters[] = {{"ERROR",   0x0001, true,  true},
                                      {"FAILURE", 0x0002, true,  true},
                                      {"WARNING", 0x0004, true,  true},
                                      {"INFO",    0x0008, false, true},
                                      {"DEBUG",   0x0010, false, true},
                                      {"ENTER",   0x0020, false, true},
                                      {"EXIT",    0x0040, false, true},
                                      {"DUMP",    0x0080, false, true}};
#endif
static const int _numLevels = sizeof(_levelFilters)/sizeof(LevelFilter);
static unsigned _maxLevelNameLength = 0;
static unsigned _globalLevel = 0;
static unsigned TL_DEFAULT = 0;
static unsigned TL_ALL = 0;
static unsigned TL_UNMASKABLE = 0;

#ifndef TF_MAX_FILE_FILTERS
#define TF_MAX_FILE_FILTERS 500
#endif
static int _numFileFilters = 0;
static FileFilter _fileFilters[TF_MAX_FILE_FILTERS];

#ifndef TF_MAX_FUNCTION_FILTERS
#define TF_MAX_FUNCTION_FILTERS 500
#endif
static int _numFunctionFilters = 0;
static FunctionFilter _functionFilters[TF_MAX_FUNCTION_FILTERS];

#ifndef TF_MAX_THREAD_FILTERS
#define TF_MAX_THREAD_FILTERS 100
#endif
static int _numThreadFilters = 0;
static ThreadFilter _threadFilters[TF_MAX_THREAD_FILTERS];
static int _numRegisteredThreads = 0;
static RegisteredThread _registeredThreads[TF_MAX_THREAD_FILTERS];
static unsigned _maxThreadNameLength = 0;

static char _configFile[180];
static const char *_prefix1 = "                     : ";
static const char *_prefix2 = " [";
static const char *_prefix3 = ", ";
static const char *_prefix4 = ":";
static const char *_prefix5 = ",";

#ifdef TF_FAST_FILENAME_LOOKUP
#define TF_MAX_SYMBOL_SCREEN_WIDTH 80
static unsigned _maxSymbolLength = 0;
static unsigned _maxSymbolColumns = 0;
#endif

#ifndef TF_FAST_FILENAME_LOOKUP
#define TF_USE_COLORS
#endif

#ifdef TF_USE_COLORS
#if 0   /* comment out the unused colors so we don't get compiler warnings */
static const char *GREEN     = "\33[1;32m";
static const char *BLUE      = "\33[1;34m";
static const char *YELLOW    = "\33[1;33m";
static const char *CYAN      = "\33[1;36m";
static const char *MAGENTA   = "\33[1;35m";
static const char *BOLD      = "\33[1m";
static const char *UNDERLINE = "\33[4m";
static const char *BLINK     = "\33[5m";
static const char *INVERSE   = "\33[7m";
#endif
static const char *RED    = "\33[1;31m";
static const char *NORMAL = "\33[0m";
static const char *ON     = "\33[1;32mON\33[0m";     /* GREEN */
static const char *OFF    = "\33[1;31mOFF\33[0m";    /* RED */
static const char *NONE   = "\33[1;36mNONE\33[0m";   /* CYAN */
#else
static const char *RED    = "";
static const char *NORMAL = "";
static const char *ON     = "ON";
static const char *OFF    = "OFF";
static const char *NONE   = "NONE";
#endif

static bool _traceEnabled = true;
static bool _filterEnabled = false;
static bool _localFilterEnabled = false;
static bool _fileFilterEnabled = false;
static bool _functionFilterEnabled = false;
static bool _threadFilterEnabled = false;
static bool _globalFilterEnabled = true;

static const char *_watchSymbol = NULL;
static void *_watchAddress = NULL;
static int _watchWidth = 0;
static long long _watchCurrValue = 0;
static long long _watchPrevValue = 0;
static const char *_watchPrevFile = NULL;
static int _watchPrevLine = 0;
static const char *_watchPrevFunction = NULL;
static int _watchNumHits = 0;
static char _watchFormatString[180];
static tf_TraceControl _watchControl = TF_ONCE;

static const char *_callbackPrevFile = NULL;
static int _callbackPrevLine = 0;
static const char *_callbackPrevFunction = NULL;
static bool _callbackPrevCondition = false;
static pthread_mutex_t _inCallback = PTHREAD_MUTEX_INITIALIZER;
static const char *_callbackName = NULL;
static tf_TraceCallback _callbackFunction = NULL;
static int _callbackNumHits = 0;
static tf_TraceControl _callbackControl = TF_ONCE;

//#define TF_PRINT(name, file, function, line, message) printf("%s: %s(%s):%d - %s\n", name, file, function, line, message)

/************************************
 * "public" function bodies
 ************************************/
 
/******************************************************************************/
/******************************************************************************/
void tf_init(const char *configFile_)
{

  /* setup trace level stuff */
  for (int i = 0; i < _numLevels; i++)
  {
    if (strlen(_levelFilters[i].name) > _maxLevelNameLength)
    {
      _maxLevelNameLength = strlen(_levelFilters[i].name);
    }
    TL_ALL |= _levelFilters[i].level;
    if (!_levelFilters[i].isMaskable)
    {
      TL_UNMASKABLE |= _levelFilters[i].level;
      TL_DEFAULT |= _levelFilters[i].level;
    }
    if (_levelFilters[i].isDefault)
    {
      TL_DEFAULT |= _levelFilters[i].level;
    }
  }
  _globalLevel = TL_DEFAULT;

#ifdef TF_FAST_FILENAME_LOOKUP
  unsigned length;
  for (int i = 0; i < TraceSymbols::_numSymbols; i++)
  {
    if ((length = strlen(TraceSymbols::_symbolTable[i])) > _maxSymbolLength)
    {
      _maxSymbolLength = length;
    }
  }
  _maxSymbolColumns = TF_MAX_SYMBOL_SCREEN_WIDTH/(_maxSymbolLength+2);
#endif

  /* register our pshell callback configuration function */
  pshell_addCommand(configureFilter,                         /* function */
                    "trace",                                 /* command */
                    "configure the dynamic trace settings",  /* description */
                    "{on | off} |\n"
#ifdef TF_FAST_FILENAME_LOOKUP
                    "             show {config | levels | threads [<thread>] | symbols [<symbol>]} |\n"
#else
                    "             show {config | levels | threads [<thread>]} |\n"
#endif
                    "             load [<filename>] |\n"
                    "             filter {on | off} |\n"
                    "             global {on | off | all | default | [+|-]<level> [<level>] ...} |\n"
                    "             local {on | off} |\n"
                    "             file {on | off | [+|-]<file>[:<lineSpec>][:<levelSpec>] [<file>[:<lineSpec>][:<levelSpec>]] ...} |\n"
                    "             function {on | off | [+|-]<function>[:<levelSpec>] [<function>[:<levelSpec>]] ...}\n"
                    "             thread {on | off | [+|-]<thread>[:<levelSpec>] [<thread>[:<levelSpec>]] ...}",     /* usage */
                    1,                                       /* minArgs */
                    30,                                      /* maxArgs */
                    false);                                  /* showUsage on "?" */

  /* load any startup configuration file */
  _configFile[0] = '\0';
  if ((configFile_ != NULL) && (strlen(configFile_) > 0))
  {
    loadConfigFile(configFile_, false);
  }

}

/******************************************************************************/
/******************************************************************************/
void tf_registerThread(const char *threadName_)
{
  unsigned length;
  if ((findRegisteredThread(threadName_) == NULL) &&
      (_numRegisteredThreads < TF_MAX_THREAD_FILTERS))
  {
    if ((length = strlen(threadName_)) > _maxThreadNameLength)
    {
      _maxThreadNameLength = length;
    }
    _registeredThreads[_numRegisteredThreads].threadName = strdup(threadName_);
    _registeredThreads[_numRegisteredThreads].threadId = pthread_self();
    _numRegisteredThreads++;
  }
}

/******************************************************************************/
/******************************************************************************/
bool tf_isFilterPassed(const char *file_,
                       int line_,
                       const char *function_,
                       unsigned level_)
{
  FileFilter *fileFilter = NULL;
  FunctionFilter *functionFilter = NULL;
  ThreadFilter *threadFilter = NULL;
  bool fileFilterPassed = !_fileFilterEnabled;
  bool functionFilterPassed = !_functionFilterEnabled;
  bool threadFilterPassed = !_threadFilterEnabled;
  bool filterPassed = false;
  char traceOutputString[180];
#ifdef TF_NATIVE_DISCRETE_LEVELS
  /* no need to translate level, use native discrete bitmask level directly */
  unsigned level = level_;
#else
  /* translate our hierarchical level to a discrete bitmask level */
  unsigned level = _levelFilters[level_].level;
#endif
  if (isWatchHit() && ((_watchNumHits == 0) || (_watchControl != TF_ONCE)))
  {
    /* print out the most recent trace for which the value was unchanged */
    sprintf(traceOutputString, _watchFormatString, "prev", _watchPrevValue);
    trace_outputLog("WATCH", _watchPrevFile, _watchPrevFunction, _watchPrevLine, traceOutputString);
    /* print out the current trace where the value change was detected */
    sprintf(traceOutputString, _watchFormatString, "curr", _watchCurrValue);
    trace_outputLog("WATCH", file_, function_, line_, traceOutputString);
    _watchPrevValue = _watchCurrValue;
    _watchNumHits++;
    filterPassed = false;
    /* see if they requested an abort on the condition being met */
    if (_watchControl == TF_ABORT)
    {
      trace_outputLog("WATCH", __FILE__, __FUNCTION__, __LINE__, "Watchpoint requested ABORT");
      assert(0);
    }
  }
  else if (level & TL_UNMASKABLE)
  {
    /* short circuit the evaluation if the level is one of our unmaskable levels */
    filterPassed = true;
  }
  else if (!_traceEnabled)
  {
    /* if trace is not enabled, return false */
    filterPassed = false;
  }
  else if (!_filterEnabled)
  {
    /* filter not enabled, return legacy default hierarchical behavour */
    filterPassed = true;
  }
  else if (_localFilterEnabled)
  {
    /* evaluate any file filters,  need to offset the filename by 7 characters
     * because they all have a '../src/' pre-pended to it */
    if ((_fileFilterEnabled) &&
        ((fileFilter = findFileFilter(file_)) != NULL) &&
        ((fileFilterPassed = ((fileFilter->numLineFilters == 0) &&
         (fileFilter->level & level))) == false))
    {
      /* found a file filter but it did not pass, check any line filters for a pass */
      for (int i = 0; i < fileFilter->numLineFilters; i++)
      {
        if ((line_ >= fileFilter->lineFilters[i].minLine) &&
            (line_ <= fileFilter->lineFilters[i].maxLine))
        {
          fileFilterPassed = (fileFilter->level & level);
          break;
        }
      }
    }
    /* see if we have any function filters */
    if (_functionFilterEnabled)
    {
      functionFilterPassed = (((functionFilter = findFunctionFilter(function_)) != NULL) &&
                              (functionFilter->level & level));
    }
    /* see if we have any thread filters */
    if (_threadFilterEnabled)
    {
      threadFilterPassed = (((threadFilter = findThreadFilter(pthread_self())) != NULL) &&
                            (threadFilter->level & level));
    }
    /* return the results */
    if ((functionFilter == NULL) && (fileFilter == NULL) && (threadFilter == NULL))
    {
      /* did not find any filter for either the file or function, apply the global filter */
      filterPassed = (_globalFilterEnabled && (level & _globalLevel));
    }
    else
    {
      /* found at least one matching filter for either the file or function,
         return the results of the filter evaluation */
      filterPassed = (fileFilterPassed && functionFilterPassed && threadFilterPassed);
     }
  }
  else if (_globalFilterEnabled)
  {
    filterPassed = (level & _globalLevel);
  }
  else
  {
    /* in the unusual event that overall  filtering is on, but both
       local & global filtering are off, just return false */
    filterPassed = false;
  }

  /* if we have a callback function registered, call it,
     prevent an infinite recursion in case the callback is trying to issue a trace */
  if ((_callbackFunction != NULL) && (pthread_mutex_trylock(&_inCallback) == 0))
  {
    /* if the callback returns 'true', it means the condition they were monitoring
       has been met, if so, print out the previous trace and the current trace*/
    if (_callbackFunction() && ((_callbackNumHits == 0) || (_callbackControl != TF_ONCE)))
    {
      if (_callbackPrevCondition == false)
      {
        /* print out the most recent trace for which the condition was not detected */
        sprintf(traceOutputString,
                "Callback condition %s: Function: %s",
                ((_callbackPrevCondition) ? "TRUE" : "FALSE"),
                _callbackName);
                
        trace_outputLog("CALLBACK",
                        _callbackPrevFile,
                        _callbackPrevFunction,
                        _callbackPrevLine,
                        traceOutputString);

        /* print out the current trace where the condition was first detected */
        sprintf(traceOutputString,
                "Callback condition TRUE: Function: %s",
                _callbackName);
                
        trace_outputLog("CALLBACK", file_, function_, line_, traceOutputString);

        /* see if they requested an abort on the condition being met */
        if (_callbackControl == TF_ABORT)
        {
          sprintf(traceOutputString, "Callback requested ABORT: Function: %s", _callbackName);
          trace_outputLog("CALLBACK", __FILE__, __FUNCTION__, __LINE__, traceOutputString);
          assert(0);
        }
        _callbackNumHits++;
        _callbackPrevCondition = true;
        /* force trace to  be off because we are supersceding it with out own trace output */
        filterPassed = false;
      }
    }
    else if ((_callbackNumHits == 0) || (_callbackControl != TF_ONCE))
    {
      if (_callbackPrevCondition == true)
      {
        /* print out the most recent trace for which the condition was not detected */
        sprintf(traceOutputString,
                "Callback condition %s: Function: %s",
                ((_callbackPrevCondition) ? "TRUE" : "FALSE"),
                _callbackName);
                
        trace_outputLog("CALLBACK",
                        _callbackPrevFile,
                        _callbackPrevFunction,
                        _callbackPrevLine,
                        traceOutputString);

        /* print out the current trace where the condition was first detected */
        sprintf(traceOutputString, "Callback condition FALSE: Function: %s", _callbackName);
        trace_outputLog("CALLBACK", file_, function_, line_, traceOutputString);

        /* see if they requested an abort on the condition being met */
        if (_callbackControl == TF_ABORT)
        {
          sprintf(traceOutputString, "Callback requested ABORT: Function: %s", _callbackName);
          trace_outputLog("CALLBACK", __FILE__, __FUNCTION__, __LINE__, traceOutputString);
          assert(0);
        }
        _callbackNumHits++;
        _callbackPrevCondition = false;
        /* force trace to  be off because we are supersceding it with out own trace output */
        filterPassed = false;
      }
    }

    /* save off the previous trace location info */
    _callbackPrevFile = file_;
    _callbackPrevFunction = function_;
    _callbackPrevLine = line_;

    pthread_mutex_unlock(&_inCallback);
  }

  /* if we have a watchpoint enabled, remember our previous, file/function/line info */
  if (_watchSymbol != NULL)
  {
    _watchPrevFile = file_;
    _watchPrevFunction = function_;
    _watchPrevLine = line_;
  }
  
  /* if we have a callback enabled, remember our previous, file/function/line info */
  if (_callbackFunction != NULL)
  {
    _callbackPrevFile = file_;
    _callbackPrevFunction = function_;
    _callbackPrevLine = line_;
  }
  
  return (filterPassed);
  
}

/******************************************************************************/
/******************************************************************************/
void tf_watch(const char *file_,
              int line_,
              const char *function_,
              const char *symbol_,
              void *address_,
              int width_,
              const char *format_,
              tf_TraceControl control_)
{
  char traceOutputString[180];
  if (symbol_ == NULL)
  {
    trace_outputLog("WATCH", file_, function_, line_, "Watchpoint NOT SET: Symbol is NULL!!");
  }
  else if (address_ == NULL)
  {
    sprintf(traceOutputString, "Watchpoint NOT SET for Symbol: %s, Address is NULL!!", symbol_);
    trace_outputLog("WATCH", file_, function_, line_, traceOutputString);
  }
  else if ((width_ != 1) && (width_ != 2) && (width_ != 4) && (width_ != 8))
  {
    sprintf(traceOutputString,
            "Watchpoint NOT SET for Symbol: %s, Address: %p, Invalid Width: %d, must be 1, 2, 4, or 8!!",
            symbol_,
            address_,
            width_);
    trace_outputLog("WATCH", file_, function_, line_, traceOutputString);
  }
  else
  {
    /* parameters validated, ok to add watchpoint */
    _watchSymbol = symbol_;
    _watchAddress = address_;
    _watchWidth = width_;
    _watchNumHits = 0;
    getWatchValue();
    _watchPrevValue = _watchCurrValue;
    _watchPrevFile = file_;
    _watchPrevFunction = function_;
    _watchPrevLine = line_;
    _watchControl = control_;
    sprintf(_watchFormatString,
            "Watchpoint SET: Symbol: %s, Address: %p, Width: %d, Value: %s",
            _watchSymbol,
            _watchAddress,
            _watchWidth,
            format_);
    sprintf(traceOutputString, _watchFormatString, _watchCurrValue);
    trace_outputLog("WATCH", file_, function_, line_, traceOutputString);
    sprintf(_watchFormatString,
            "Watchpoint HIT: Symbol: %s, Address: %p, Value[%s]: %s",
            _watchSymbol,
            _watchAddress,
            "%s",
            format_);
  }
}

/******************************************************************************/
/******************************************************************************/
void tf_callback(const char *file_,
                 int line_,
                 const char *function_,
                 const char *callbackName_,
                 tf_TraceCallback callbackFunction_,
                 tf_TraceControl control_)
{
  char traceOutputString[180];
  _callbackName = callbackName_;
  _callbackFunction = callbackFunction_;
  _callbackPrevFile = file_;
  _callbackPrevFunction = function_;
  _callbackPrevLine = line_;
  _callbackControl = control_;
  sprintf(traceOutputString, "Callback REGISTERED: Function: %s", _callbackName);
  trace_outputLog("CALLBACK", file_, function_, line_, traceOutputString);
}

/************************************
 * "private" function bodies
 ************************************/

/******************************************************************************/
/******************************************************************************/
void getWatchValue(void)
{
  switch (_watchWidth)
  {
    case 1:
      _watchCurrValue = *(unsigned char *)_watchAddress;
      break;
    case 2:
      _watchCurrValue = *(unsigned short *)_watchAddress;
      break;
    case 4:
      _watchCurrValue = *(unsigned int *)_watchAddress;
      break;
    case 8:
      _watchCurrValue = *(unsigned long long *)_watchAddress;
      break;
    default:
      break;
  }
}

/******************************************************************************/
/******************************************************************************/
bool isWatchHit(void)
{
  if (_watchSymbol != NULL)
  {
    getWatchValue();
    return (_watchCurrValue != _watchPrevValue);
  }
  else
  {
    return (false);
  }
}

/******************************************************************************/
/******************************************************************************/
void showConfig(void)
{
  const char *prefix1 = "";
  const char *prefix2 = "";
  pshell_printf("\n");
  pshell_printf("********************************\n");
  pshell_printf("*  TRACE FILTER CONFIGURATION  *\n");
  pshell_printf("********************************\n");
  pshell_printf("\n");
  pshell_printf("Config file..........: %s\n", ((_configFile[0] == '\0') ? NONE : _configFile));
  pshell_printf("Trace enabled........: %s\n", ((_traceEnabled) ? ON : OFF));
  if (_watchSymbol != NULL)
  {
    pshell_printf("Trace watchpoint.....: %s\n", _watchSymbol);
    pshell_printf("  Address............: %p\n", _watchAddress);
    pshell_printf("  Width..............: %d byte(s)\n", _watchWidth);
    pshell_printf("  Num Hits...........: %d\n", _watchNumHits);
    pshell_printf("  Control............: ");
    if (_watchControl == TF_ONCE)
    {
      pshell_printf("ONCE\n");
    }
    else if (_watchControl == TF_CONTINUOUS)
    {
      pshell_printf("CONTINUOUS\n");
    }
    else  /* TF_ABORT */
    {
      pshell_printf("ABORT\n");
    }
  }
  else
  {
    pshell_printf("Trace watchpoint.....: %s\n", NONE);
  }
  if (_callbackFunction != NULL)
  {
    pshell_printf("Trace callback.......: %s\n", _callbackName);
    pshell_printf("  Num hits...........: %d\n", _callbackNumHits);
    pshell_printf("  Control............: ");
    if (_callbackControl == TF_ONCE)
    {
      pshell_printf("ONCE\n");
    }
    else if (_callbackControl == TF_CONTINUOUS)
    {
      pshell_printf("CONTINUOUS\n");
    }
    else  /* TF_ABORT */
    {
      pshell_printf("ABORT\n");
    }
  }
  else
  {
    pshell_printf("Trace callback.......: %s\n", NONE);
  }
  pshell_printf("Filter enabled.......: %s\n", ((_filterEnabled) ? ON : OFF));
  pshell_printf("  Local filter.......: %s\n", ((_localFilterEnabled) ? ON : OFF));
  pshell_printf("    File filter......: %s\n", ((_fileFilterEnabled) ? ON : OFF));
  if (_numFileFilters == 0)
  {
    pshell_printf("      File(s)........: %s\n", NONE);
  }
  else
  {
    pshell_printf("      File(s)........: ");
    for (int i = 0; i < _numFileFilters; i++)
    {
      pshell_printf("%s%s", prefix1, _fileFilters[i].filename);
      prefix2 = _prefix4;
      for (int j = 0; j < _fileFilters[i].numLineFilters; j++)
      {
        if (_fileFilters[i].lineFilters[j].minLine == _fileFilters[i].lineFilters[j].maxLine)
        {
          pshell_printf("%s%d", prefix2, _fileFilters[i].lineFilters[j].minLine);
        }
        else
        {
          pshell_printf("%s%d-%d", prefix2, _fileFilters[i].lineFilters[j].minLine, _fileFilters[i].lineFilters[j].maxLine);
        }
        prefix2 = _prefix5;
      }
      if (_fileFilters[i].level != TL_ALL)
      {
        prefix2 = _prefix2;
        for (int j = 0; j < _numLevels; j++)
        {
          if (_fileFilters[i].level & _levelFilters[j].level)
          {
            pshell_printf("%s%s", prefix2, _levelFilters[j].name);
            prefix2 = _prefix3;
          }
        }
        pshell_printf("]");
      }
      pshell_printf("\n");
      prefix1 = _prefix1;
    }
  }
  prefix1 = "";
  pshell_printf("    Function filter..: %s\n", ((_functionFilterEnabled) ? ON : OFF));
  if (_numFunctionFilters == 0)
  {
    pshell_printf("      Function(s)....: %s\n", NONE);
  }
  else
  {
    pshell_printf("      Function(s)....: ");
    for (int i = 0; i < _numFunctionFilters; i++)
    {
      pshell_printf("%s%s ", prefix1, _functionFilters[i]);
      if (_functionFilters[i].level != TL_ALL)
      {
        prefix2 = _prefix2;
        for (int j = 0; j < _numLevels; j++)
        {
          if (_functionFilters[i].level & _levelFilters[j].level)
          {
            pshell_printf("%s%s", prefix2, _levelFilters[j].name);
            prefix2 = _prefix3;
          }
        }
        pshell_printf("]");
      }
      pshell_printf("\n");
      prefix1 = _prefix1;
    }
  }
  prefix1 = "";
  pshell_printf("    Thread filter....: %s\n", ((_threadFilterEnabled) ? ON : OFF));
  if (_numThreadFilters == 0)
  {
    pshell_printf("      Thread(s)......: %s\n", NONE);
  }
  else
  {
    pshell_printf("      Thread(s)......: ");
    for (int i = 0; i < _numThreadFilters; i++)
    {
      pshell_printf("%s%s ", prefix1, _threadFilters[i].threadName);
      if (_threadFilters[i].level != TL_ALL)
      {
        prefix2 = _prefix2;
        for (int j = 0; j < _numLevels; j++)
        {
          if (_threadFilters[i].level & _levelFilters[j].level)
          {
            pshell_printf("%s%s", prefix2, _levelFilters[j].name);
            prefix2 = _prefix3;
          }
        }
        pshell_printf("]");
      }
      pshell_printf("\n");
      prefix1 = _prefix1;
    }
  }
  pshell_printf("  Global filter......: %s\n", ((_globalFilterEnabled) ? ON : OFF));
  prefix1 = "";
  pshell_printf("    Level(s).........: ");
  for (int i = 0; i < _numLevels; i++)
  {
    if (_levelFilters[i].level & _globalLevel)
    {
      pshell_printf("%s%s\n", prefix1, _levelFilters[i].name);
      prefix1 = _prefix1;
    }
  }
  pshell_printf("\n");
}

/******************************************************************************/
/******************************************************************************/
void showUsage(void)
{
  pshell_printf("\n");
  pshell_showUsage();
  pshell_printf("\n");
  pshell_printf("  where:\n");
#ifdef TF_FAST_FILENAME_LOOKUP
  pshell_printf("    <symbol>    - the symbol (i.e.file) name or substring\n");
#endif
  pshell_printf("    <thread>    - the registered thread name or substring\n");
  pshell_printf("    <level>     - one of the available trace levels\n");
  pshell_printf("    <lineSpec>  - list of one or more lines to filter (e.g. 1,3,5-7,9)\n");
  pshell_printf("    <levelSpec> - list of one or more levels or 'default' (e.g. enter,exit)\n");
  pshell_printf("    +           - append the filter item to the specified list\n");
  pshell_printf("    -           - remove the filter item from the specified list\n");
  pshell_printf("\n");
  pshell_printf("  NOTE: If no '+' or '-' is given, the filter is set to the entered list\n");
  pshell_printf("\n");
}

/******************************************************************************/
/******************************************************************************/
void configureFilter(int argc, char *argv[])
{

  if (pshell_isHelp())
  {
    showUsage();
  }
  else if (pshell_isSubString(argv[0], "file", 4) && (argc > 1))
  {
    if (pshell_isSubString(argv[1], "on", 2))
    {
      _fileFilterEnabled = true;
    }
    else if (pshell_isSubString(argv[1], "off", 2))
    {
      _fileFilterEnabled = false;
    }
    else if (argv[1][0] == '+')
    {
      /* append to existing file filters */
      addFileFilter(&argv[1][1], true);
      for (int i = 2; i < argc; i++)
      {
        addFileFilter(argv[i], true);
      }
    }
    else if (argv[1][0] == '-')
    {
      /* remove from existing file filters */
      removeFileFilter(&argv[1][1]);
      for (int i = 2; i < argc; i++)
      {
        removeFileFilter(argv[i]);
      }
    }
    else
    {
      /* set file filters to specified list */
      removeAllFileFilters();
      for (int i = 1; i < argc; i++)
      {
        addFileFilter(argv[i], true);
      }
    }
  }
  else if (pshell_isSubString(argv[0], "function", 4) && (argc > 1))
  {
    if (pshell_isSubString(argv[1], "on", 2))
    {
      _functionFilterEnabled = true;
    }
    else if (pshell_isSubString(argv[1], "off", 2))
    {
      _functionFilterEnabled = false;
    }
    else if (argv[1][0] == '+')
    {
      /* append to existing function filters */
      addFunctionFilter(&argv[1][1], true);
      for (int i = 2; i < argc; i++)
      {
        addFunctionFilter(argv[i], true);
      }
    }
    else if (argv[1][0] == '-')
    {
      /* remove from existing function filters */
      removeFunctionFilter(&argv[1][1]);
      for (int i = 2; i < argc; i++)
      {
        removeFunctionFilter(argv[i]);
      }
    }
    else
    {
      /* set function filters to specified list */
      removeAllFunctionFilters();
      for (int i = 1; i < argc; i++)
      {
        addFunctionFilter(argv[i], true);
      }
    }
  }
  else if (pshell_isSubString(argv[0], "thread", 1) && (argc > 1))
  {
    if (pshell_isSubString(argv[1], "on", 2))
    {
      _threadFilterEnabled = true;
    }
    else if (pshell_isSubString(argv[1], "off", 2))
    {
      _threadFilterEnabled = false;
    }
    else if (argv[1][0] == '+')
    {
      /* append to existing thread filters */
      addThreadFilter(&argv[1][1], true);
      for (int i = 2; i < argc; i++)
      {
        addThreadFilter(argv[i], true);
      }
    }
    else if (argv[1][0] == '-')
    {
      /* remove from existing thread filters */
      removeThreadFilter(&argv[1][1]);
      for (int i = 2; i < argc; i++)
      {
        removeThreadFilter(argv[i]);
      }
    }
    else
    {
      /* set thread filters to specified list */
      removeAllThreadFilters();
      for (int i = 1; i < argc; i++)
      {
        addThreadFilter(argv[i], true);
      }
    }
  }
  else if (pshell_isSubString(argv[0], "filter", 4) && (argc == 2))
  {
    if (pshell_isSubString(argv[1], "on", 2))
    {
      _filterEnabled = true;
    }
    else if (pshell_isSubString(argv[1], "off", 2))
    {
      _filterEnabled = false;
    }
    else
    {
      showUsage();
    }
  }
  else if (pshell_isSubString(argv[0], "show", 1) && (argc > 1))
  {
    if (pshell_isSubString(argv[1], "config", 1))
    {
      showConfig();
    }
    else if (pshell_isSubString(argv[1], "levels", 1))
    {
      showLevels();
    }
    else if (pshell_isSubString(argv[1], "threads", 1))
    {
      if (argc == 3)
      {
        showThreads(argv[2]);
      }
      else
      {
        showThreads(NULL);
      }
    }
#ifdef TF_FAST_FILENAME_LOOKUP
    else if (pshell_isSubString(argv[1], "symbols", 1))
    {
      if (argc == 3)
      {
        showSymbols(argv[2]);
      }
      else
      {
        showSymbols(NULL);
      }
    }
#endif
    else
    {
      showUsage();
    }
  }
  else if (pshell_isSubString(argv[0], "global", 1) && (argc > 1))
  {
    _filterEnabled = true;
    if (pshell_isSubString(argv[1], "on", 2) && (argc == 2))
    {
      _globalFilterEnabled = true;
    }
    else if (pshell_isSubString(argv[1], "off", 2) && (argc == 2))
    {
      _globalFilterEnabled = false;
    }
    else if (pshell_isSubString(argv[1], "all", 1) && (argc == 2))
    {
      _globalLevel = TL_ALL;
    }
    else if (pshell_isSubString(argv[1], "default", 3) && (argc == 2))
    {
      _globalLevel = TL_DEFAULT;
    }
    else if (argv[1][0] == '+')
    {
      /* append to existing level filters */
      addLevelFilter(&argv[1][1], _globalLevel);
      for (int i = 2; i < argc; i++)
      {
        addLevelFilter(argv[i], _globalLevel);
      }
    }
    else if (argv[1][0] == '-')
    {
      /* remove from existing level filters */
      removeLevelFilter(&argv[1][1], _globalLevel);
      for (int i = 2; i < argc; i++)
      {
        removeLevelFilter(argv[i], _globalLevel);
      }
    }
    else
    {
      /* set level filters to specified list */
      _globalLevel = TL_UNMASKABLE;
      for (int i = 1; i < argc; i++)
      {
        addLevelFilter(argv[i], _globalLevel);
      }
    }
  }
  else if (pshell_isSubString(argv[0], "on", 2) && (argc = 1))
  {
    _traceEnabled = true;
  }
  else if (pshell_isSubString(argv[0], "off", 2) && (argc = 1))
  {
    _traceEnabled = false;
  }
  else if (pshell_isSubString(argv[0], "local", 3) && (argc == 2))
  {
    if (pshell_isSubString(argv[1], "on", 2))
    {
      _localFilterEnabled = true;
    }
    else if (pshell_isSubString(argv[1], "off", 2))
    {
      _localFilterEnabled = false;
    }
    else
    {
      showUsage();
    }
  }
  else if (pshell_isSubString(argv[0], "load", 3))
  {
    if (argc == 1)
    {
      if (_configFile[0] != '\0')
      {
        loadConfigFile(_configFile, true);
      }
      else
      {
        pshell_printf("\n");
        pshell_printf("ERROR: No valid config file specified to reload\n");
        pshell_printf("\n");
      }
    }
    else if (argc == 2)
    {
      loadConfigFile(argv[1], true);
    }
    else
    {
      showUsage();
    }
  }
  else
  {
    showUsage();
  }
}

/******************************************************************************/
/******************************************************************************/
void loadConfigFile(const char *file_, bool interactive_)
{
  FILE *fp;
  char line[300];
  char configFile[180];
  char cwd[180];
  char *configPath;
  Tokens tokens;

  configFile[0] = '\0';
  if ((configPath = getenv("PSHELL_CONFIG_DIR")) != NULL)
  {
      snprintf(configFile, sizeof(configFile), "%s/%s", configPath, file_);
  }

  if ((fp = fopen(configFile, "r")) == NULL)
  {
    /* either the env variable is not found or the file is not found
     * look in our default directory
     */
    sprintf(configFile, "%s/%s", PSHELL_CONFIG_DIR, file_);
    if ((fp = fopen(configFile, "r")) == NULL)
    {
      /* not found in our default directory, look in our CWD */
      getcwd(cwd, sizeof(cwd));
      snprintf(configFile, sizeof(configFile), "%s/%s", cwd, file_);
      if ((fp = fopen(configFile, "r")) == NULL)
      {
        /* still not found, just open the filename as-is */
        sprintf(configFile, "%s", file_);
        fp = fopen(configFile, "r");
      }
    }
  }

  if (fp != NULL)
  {
    strcpy(_configFile, configFile);
    removeAllFileFilters();
    removeAllFunctionFilters();
    removeAllThreadFilters();
    _globalLevel = TL_DEFAULT;
    while (fgets(line, 300, fp) != NULL)
    {
      if ((line[0] != '#') && (line[0] != '\n'))
      {
        /* non-comment line, process it */
        line[strlen(line)-1] = '\0';
        tokenize(line, tokens, " ");
        if ((tokens.numTokens >= 2) && (strcmp(tokens.tokens[0], "trace") == 0))
        {
          if (strcmp(tokens.tokens[1], "on") == 0)
          {
            _traceEnabled = true;
          }
          else if (strcmp(tokens.tokens[1], "off") == 0)
          {
            _traceEnabled = false;
          }
          else if ((tokens.numTokens > 2) && (strcmp(tokens.tokens[1], "filter") == 0))
          {
            if (strcmp(tokens.tokens[2], "on") == 0)
            {
              _filterEnabled = true;
            }
            else if (strcmp(tokens.tokens[2], "off") == 0)
            {
              _filterEnabled = false;
            }
          }
          else if ((tokens.numTokens > 2) && (strcmp(tokens.tokens[1], "local") == 0))
          {
            if (strcmp(tokens.tokens[2], "on") == 0)
            {
              _localFilterEnabled = true;
            }
            else if (strcmp(tokens.tokens[2], "off") == 0)
            {
              _localFilterEnabled = false;
            }
          }
          else if ((tokens.numTokens > 2) && (strcmp(tokens.tokens[1], "global") == 0))
          {
            if (strcmp(tokens.tokens[2], "on") == 0)
            {
              _globalFilterEnabled = true;
            }
            else if (strcmp(tokens.tokens[2], "off") == 0)
            {
              _globalFilterEnabled = false;
            }
            else if (strcmp(tokens.tokens[2], "all") == 0)
            {
              _globalLevel = TL_ALL;
            }
            else if (strcmp(tokens.tokens[2], "default") == 0)
            {
              _globalLevel = TL_DEFAULT;
            }
            else
            {
              _globalLevel = TL_UNMASKABLE;
              for (int i = 2; i < tokens.numTokens; i++)
              {
                addLevelFilter(tokens.tokens[i], _globalLevel);
              }
            }
          }
          else if ((tokens.numTokens > 2) && (strcmp(tokens.tokens[1], "file") == 0))
          {
            if (strcmp(tokens.tokens[2], "on") == 0)
            {
              _fileFilterEnabled = true;
            }
            else if (strcmp(tokens.tokens[2], "off") == 0)
            {
              _fileFilterEnabled = false;
            }
            else
            {
              for (int i = 2; i < tokens.numTokens; i++)
              {
                addFileFilter(tokens.tokens[i], interactive_);
              }
            }
          }
          else if ((tokens.numTokens > 2) && (strcmp(tokens.tokens[1], "function") == 0))
          {
            if (strcmp(tokens.tokens[2], "on") == 0)
            {
              _functionFilterEnabled = true;
            }
            else if (strcmp(tokens.tokens[2], "off") == 0)
            {
              _functionFilterEnabled = false;
            }
            else
            {
              for (int i = 2; i < tokens.numTokens; i++)
              {
                addFunctionFilter(tokens.tokens[i], interactive_);
              }
            }
          }
          else if ((tokens.numTokens > 2) && (strcmp(tokens.tokens[1], "thread") == 0))
          {
            if (strcmp(tokens.tokens[2], "on") == 0)
            {
              _threadFilterEnabled = true;
            }
            else if (strcmp(tokens.tokens[2], "off") == 0)
            {
              _threadFilterEnabled = false;
            }
            else
            {
              for (int i = 2; i < tokens.numTokens; i++)
              {
                addThreadFilter(tokens.tokens[i], interactive_);
              }
            }
          }
        }
      }
    }
    fclose(fp);
  }
  else if (interactive_)
  {
    pshell_printf("\n");
    pshell_printf("ERROR: Could not load configuration file: %s\n", file_);
    pshell_printf("\n");
  }
}

/******************************************************************************/
/******************************************************************************/
RegisteredThread *findRegisteredThread(const char *thread_)
{
  RegisteredThread *thread = NULL;
  for (int i = 0; i < _numRegisteredThreads; i++)
  {
    if (strstr(_registeredThreads[i].threadName, thread_) != NULL)
    {
      if (thread == NULL)
      {
        thread = &_registeredThreads[i];
      }
      else
      {
        thread = NULL;
        break;
      }
    }
  }
  return (thread);
}

/******************************************************************************/
/******************************************************************************/
const char *findSymbol(const char *symbol_)
{
#ifdef TF_FAST_FILENAME_LOOKUP
  const char *symbol = NULL;
  for (int i = 0; i < TraceSymbols::_numSymbols; i++)
  {
    if (strstr(TraceSymbols::_symbolTable[i], symbol_) != NULL)
    {
      if (symbol == NULL)
      {
        symbol = TraceSymbols::_symbolTable[i];
      }
      else
      {
        symbol = NULL;
        break;
      }
    }
  }
  return (symbol);
#else
  return (symbol_);
#endif
}

#ifdef TF_FAST_FILENAME_LOOKUP

/******************************************************************************/
/******************************************************************************/
const void showSymbols(char *symbol_)
{
  bool symbolFound = false;
  int numFound = 0;
  const char *ptr;
  pshell_printf("\n");
  pshell_printf("*****************************\n");
  pshell_printf("*  AVAILABLE TRACE SYMBOLS  *\n");
  pshell_printf("*****************************\n");
  pshell_printf("\n");
  if (TraceSymbols::_numSymbols == 0)
  {
    pshell_printf("No registered file symbols\n\n");
    return;
  }
  for (int i = 0; i < TraceSymbols::_numSymbols; i++)
  {
    if (symbol_ == NULL)
    {
      pshell_printf("%-*s  ", _maxSymbolLength, TraceSymbols::_symbolTable[i]);
      if ((((i+1)%_maxSymbolColumns) == 0) || (i == (TraceSymbols::_numSymbols-1)))
      {
        pshell_printf("\n");
      }
    }
    else if ((ptr = strstr(TraceSymbols::_symbolTable[i], symbol_)) != NULL)
    {
      int numHighlighted = 0;
      int numToHighlight = (int)strlen(symbol_);
      bool highlight = false;
      int length = strlen(TraceSymbols::_symbolTable[i]);
      for (int j = 0; j < length; j++)
      {
        if (&TraceSymbols::_symbolTable[i][j] == ptr)
        {
          highlight = true;
        }
        if (highlight)
        {
          pshell_printf("%s%c%s", RED, TraceSymbols::_symbolTable[i][j], NORMAL);
          numHighlighted++;
          if (numHighlighted == numToHighlight)
          {
            highlight = false;
          }
        }
        else
        {
          pshell_printf("%c", TraceSymbols::_symbolTable[i][j]);
        }
      }
      for (int j = 0; j < (int)(_maxSymbolLength-length); j++)
      {
        pshell_printf(" ");
      }
      if (((numFound+1)%_maxSymbolColumns) == 0)
      {
        pshell_printf("\n");
      }
      else
      {
        pshell_printf("  ");
      }
      numFound++;
      symbolFound = true;
    }
  }
  if (symbol_ != NULL)
  {
    if (!symbolFound)
    {
      pshell_printf("Symbol '%s' not found, add macro 'TF_SYMBOL'TABLE' to file(s): *%s*\n", symbol_, symbol_);
      pshell_printf("\n");
    }
    else if ((numFound%_maxSymbolColumns) != 0)
    {
      pshell_printf("\n\n");
    }
    else
    {
      pshell_printf("\n");
    }
  }
  else
  {
    pshell_printf("\n");
  }
}
#endif

/******************************************************************************/
/******************************************************************************/
void addLevelFilter(char *name_, unsigned &level_)
{
  for (int i = 0; i < _numLevels; i++)
  {
    if (strcasecmp(name_, _levelFilters[i].name) == 0)
    {
      level_ |= _levelFilters[i].level;
      break;
    }
  }
  if (strncasecmp(name_, "default", 3) == 0)
  {
    level_ |= TL_DEFAULT;
  }
}

/******************************************************************************/
/******************************************************************************/
void removeLevelFilter(char *name_, unsigned &level_)
{
  for (int i = 0; i < _numLevels; i++)
  {
    if (strcasecmp(name_, _levelFilters[i].name) == 0)
    {
      if (_levelFilters[i].isMaskable)
      {
        level_ &= ~_levelFilters[i].level;
      }
      else
      {
        pshell_printf("\n");
        pshell_printf("ERROR: Level '%s' is specified as unmaskable\n", _levelFilters[i].name);
        pshell_printf("\n");
      }
      break;
    }
  }
}

/******************************************************************************/
/******************************************************************************/
const void showThreads(char *thread_)
{
  bool threadFound = false;
  int numFound = 0;
  const char *ptr;
  pshell_printf("\n");
  pshell_printf("*****************************\n");
  pshell_printf("*  AVAILABLE TRACE THREADS  *\n");
  pshell_printf("*****************************\n");
  pshell_printf("\n");
  if (_numRegisteredThreads > 0)
  {
    for (int i = 0; i < _numRegisteredThreads; i++)
    {
      if (thread_ == NULL)
      {
        pshell_printf("%s\n", _registeredThreads[i].threadName);
      }
      else if ((ptr = strstr(_registeredThreads[i].threadName, thread_)) != NULL)
      {
        int numHighlighted = 0;
        int numToHighlight = (int)strlen(thread_);
        bool highlight = false;
        int length = strlen(_registeredThreads[i].threadName);
        for (int j = 0; j < length; j++)
        {
          if (&_registeredThreads[i].threadName[j] == ptr)
          {
            highlight = true;
          }
          if (highlight)
          {
            pshell_printf("%s%c%s", RED, _registeredThreads[i].threadName[j], NORMAL);
            numHighlighted++;
            if (numHighlighted == numToHighlight)
            {
              highlight = false;
            }
          }
          else
          {
            pshell_printf("%c", _registeredThreads[i].threadName[j]);
          }
        }
        pshell_printf("\n");
        numFound++;
        threadFound = true;
      }
    }
    if ((thread_ != NULL) && (!threadFound))
    {
      pshell_printf("Thread '%s' not found, register thread\n", thread_);
      pshell_printf("with the 'tf_registerThread' function call\n");
    }
  }
  else
  {
    pshell_printf("No registered threads\n");
  }
  pshell_printf("\n");
}

/******************************************************************************/
/******************************************************************************/
const void showLevels(void)
{
  pshell_printf("\n");
  pshell_printf("****************************\n");
  pshell_printf("*  AVAILABLE TRACE LEVELS  *\n");
  pshell_printf("****************************\n");
  pshell_printf("\n");
  pshell_printf("%-*s  DEFAULT  MASKABLE\n", _maxLevelNameLength, "NAME");
  for (int i = 0; i < (int)_maxLevelNameLength; i++) pshell_printf("-");
  pshell_printf("  -------  --------\n");
  for (int i = 0; i < _numLevels; i++)
  {
    pshell_printf("%-*s  %-7s  %s\n",
                  _maxLevelNameLength,
                  _levelFilters[i].name,
                  ((_levelFilters[i].isDefault) ? "YES" : "NO"),
                  ((_levelFilters[i].isMaskable) ? "YES" : "NO"));
  }
  pshell_printf("\n");
}

/******************************************************************************/
/******************************************************************************/
bool isLevel(char *string_)
{
  for (int i = 0; i < _numLevels; i++)
  {
    if (strncasecmp(string_, _levelFilters[i].name, strlen(string_)) == 0)
    {
      return (true);
    }
  }
  return (pshell_isSubString(string_, "default", 3));
}

/******************************************************************************/
/******************************************************************************/
void addFileFilter(const char *file_, bool interactive_)
{
  const char *symbol;
  Tokens tokens;
  Tokens levelOrLineSpec;
  Tokens levelSpec;
  Tokens lineSpec;
  Tokens lineRange;
  char originalRequest[300];
  char file[300];
  strcpy(originalRequest, file_);
  strcpy(file, file_);
  tokenize(file, tokens, ":");
  FileFilter *filter;
  char *ptr;
  if (_numFileFilters < TF_MAX_FILE_FILTERS)
  {
    if ((symbol = findSymbol(tokens.tokens[0])) != NULL)
    {
      _filterEnabled = true;
      _localFilterEnabled = true;
      _fileFilterEnabled = true;
      /* see if we already have a filter setup for this file */
      if ((filter = findFileFilter(symbol)) == NULL)
      {
        /* filter does not exist for this file, add a new one */
        filter = &_fileFilters[_numFileFilters++];
#ifdef TF_FAST_FILENAME_LOOKUP
        filter->filename = symbol;
#else
        filter->filename = strdup(symbol);
#endif
      }
      filter->numLineFilters = 0;
      /* see if we have a lineSpec and/or levelSpec */
      if (tokens.numTokens == 3)
      {
        /* we have both a lineSpec and levelSpec */
        if ((ptr = strrchr(originalRequest, ':')) != NULL)
        {
          *ptr = 0;
        }
        /* process the lineSpec */
        tokenize(tokens.tokens[1], lineSpec, ",");
        for (int i = 0; i < lineSpec.numTokens; i++)
        {
          if (filter->numLineFilters < TF_MAX_LINE_FILTERS)
          {
            /* see if we have a line range */
            tokenize(lineSpec.tokens[i], lineRange, "-");
            if (lineRange.numTokens == 1)
            {
              /* single line specified, set minLine and maxLine to the same value */
              filter->lineFilters[filter->numLineFilters].minLine = atoi(lineRange.tokens[0]);
              filter->lineFilters[filter->numLineFilters].maxLine = atoi(lineRange.tokens[0]);
              filter->numLineFilters++;
            }
            else if (lineRange.numTokens == 2)
            {
              /* line range specified */
              filter->lineFilters[filter->numLineFilters].minLine = atoi(lineRange.tokens[0]);
              filter->lineFilters[filter->numLineFilters].maxLine = atoi(lineRange.tokens[1]);
              filter->numLineFilters++;
            }
          }
          else
          {
            if (interactive_)
            {
              pshell_printf("\n");
              pshell_printf("Max line filters: %d, exceeded\n", TF_MAX_LINE_FILTERS);
              pshell_printf("\n");
            }
            break;
          }
        }
        /* process the levelSpec */
        tokenize(tokens.tokens[2], levelSpec, ",");
        filter->level = TL_UNMASKABLE;
        if ((levelSpec.numTokens == 1) && pshell_isSubString(levelSpec.tokens[0], "default", 3))
        {
          filter->level = TL_DEFAULT;
        }
        else
        {
          for (int i = 0; i < levelSpec.numTokens; i++)
          {
            addLevelFilter(levelSpec.tokens[i], filter->level);
          }
        }
      }
      else if (tokens.numTokens == 2)
      {
        /* only 2 tokens, see if the second one is a lineSpec of levelSpec */
        tokenize(tokens.tokens[1], levelOrLineSpec, ",");
        if (isLevel(levelOrLineSpec.tokens[0]))
        {
          if ((ptr = strrchr(originalRequest, ':')) != NULL)
          {
            *ptr = 0;
          }
          /* process the levelSpec */
          filter->level = TL_UNMASKABLE;
          if ((levelOrLineSpec.numTokens == 1) && pshell_isSubString(levelOrLineSpec.tokens[0], "default", 3))
          {
            filter->level = TL_DEFAULT;
          }
          else
          {
            for (int i = 0; i < levelOrLineSpec.numTokens; i++)
            {
              addLevelFilter(levelOrLineSpec.tokens[i], filter->level);
            }
          }
        }
        else
        {
          filter->level = TL_ALL;
          /* process the lineSpec */
          for (int i = 0; i < levelOrLineSpec.numTokens; i++)
          {
            if (filter->numLineFilters < TF_MAX_LINE_FILTERS)
            {
              /* see if we have a line range */
              tokenize(levelOrLineSpec.tokens[i], lineRange, "-");
              if (lineRange.numTokens == 1)
              {
                /* single line specified, set minLine and maxLine to the same value */
                filter->lineFilters[filter->numLineFilters].minLine = atoi(lineRange.tokens[0]);
                filter->lineFilters[filter->numLineFilters].maxLine = atoi(lineRange.tokens[0]);
                filter->numLineFilters++;
              }
              else if (lineRange.numTokens == 2)
              {
                /* line range specified */
                filter->lineFilters[filter->numLineFilters].minLine = atoi(lineRange.tokens[0]);
                filter->lineFilters[filter->numLineFilters].maxLine = atoi(lineRange.tokens[1]);
                filter->numLineFilters++;
              }
            }
            else
            {
              if (interactive_)
              {
                pshell_printf("\n");
                pshell_printf("Max line filters: %d, exceeded\n", TF_MAX_LINE_FILTERS);
                pshell_printf("\n");
              }
              break;
            }
          }
        }
      }
      else if (tokens.numTokens == 1)
      {
        filter->level = TL_ALL;
      }
    }
    else if (interactive_)
    {
      pshell_printf("\n");
      pshell_printf("Symbol '%s' not found or ambiguous, add macro\n", tokens.tokens[0]);
      pshell_printf("'TF_SYMBOL'TABLE' to file or expand abbreviation\n");
      pshell_printf("\n");
     }
  }
  else if (interactive_)
  {
    pshell_printf("\n");
    pshell_printf("Max file filters: %d, exceeded\n", TF_MAX_FILE_FILTERS);
    pshell_printf("\n");
  }
}

/******************************************************************************/
/******************************************************************************/
void addFunctionFilter(const char *function_, bool interactive_)
{
  Tokens tokens;
  Tokens levels;
  FunctionFilter *filter;
  bool newFilter = false;
  if (_numFunctionFilters < TF_MAX_FUNCTION_FILTERS)
  {
    tokenize((char *)function_, tokens, ":");
    if ((filter = findFunctionFilter(tokens.tokens[0])) == NULL)
    {
      filter = &_functionFilters[_numFunctionFilters];
      newFilter = true;
    }
    _filterEnabled = true;
    _localFilterEnabled = true;
    _functionFilterEnabled = true;
    /* see if they specified any custom levels */
    if (tokens.numTokens > 1)
    {
      filter->level = TL_UNMASKABLE;
      tokenize(tokens.tokens[1], levels, ",");
      if ((levels.numTokens == 1) && pshell_isSubString(levels.tokens[0], "default", 3))
      {
        filter->level = TL_DEFAULT;
      }
      else
      {
        for (int i = 0; i < levels.numTokens; i++)
        {
          addLevelFilter(levels.tokens[i], filter->level);
        }
      }
    }
    else
    {
      /* no custom levels specified, default fo ALL levels */
      filter->level = TL_ALL;
    }
    if (newFilter)
    {
      filter->functionName = strdup(tokens.tokens[0]);
      _numFunctionFilters++;
    }
  }
  else if (interactive_)
  {
    pshell_printf("\n");
    pshell_printf("Max function filters: %d, exceeded\n", TF_MAX_FUNCTION_FILTERS);
    pshell_printf("\n");
  }
}

/******************************************************************************/
/******************************************************************************/
void addThreadFilter(const char *thread_, bool interactive_)
{
  Tokens tokens;
  Tokens levels;
  ThreadFilter *filter;
  RegisteredThread *thread;
  bool newFilter = false;
  if (_numThreadFilters < TF_MAX_THREAD_FILTERS)
  {
    tokenize((char *)thread_, tokens, ":");
    if ((thread = findRegisteredThread(tokens.tokens[0])) != NULL)
    {
      if ((filter = findThreadFilter(thread->threadName)) == NULL)
      {
        filter = &_threadFilters[_numThreadFilters];
        newFilter = true;
      }
      _filterEnabled = true;
      _localFilterEnabled = true;
      _threadFilterEnabled = true;
      /* see if they specified any custom levels */
      if (tokens.numTokens > 1)
      {
        filter->level = TL_UNMASKABLE;
        tokenize(tokens.tokens[1], levels, ",");
        if ((levels.numTokens == 1) && pshell_isSubString(levels.tokens[0], "default", 3))
        {
          filter->level = TL_DEFAULT;
        }
        else
        {
          for (int i = 0; i < levels.numTokens; i++)
          {
            addLevelFilter(levels.tokens[i], filter->level);
          }
        }
      }
      else
      {
        /* no custom levels specified, default fo ALL levels */
        filter->level = TL_ALL;
      }
      if (newFilter)
      {
        filter->threadName = thread->threadName;
        filter->threadId = thread->threadId;
        _numThreadFilters++;
      }
    }
    else if (interactive_)
    {
      pshell_printf("\n");
      pshell_printf("Thread '%s' not found or ambiguous, register thread\n", tokens.tokens[0]);
      pshell_printf("with the 'tf_registerThread' function call\n");
      pshell_printf("\n");
    }
  }
  else if (interactive_)
  {
    pshell_printf("\n");
    pshell_printf("Max thread filters: %d, exceeded\n", TF_MAX_THREAD_FILTERS);
    pshell_printf("\n");
  }
}

/******************************************************************************/
/******************************************************************************/
FileFilter *findFileFilter(const char *file_)
{
  for (int i = 0; i < _numFileFilters; i++)
  {
#ifdef TF_FAST_FILENAME_LOOKUP
    /* for fast filename all we need to do is just compare the pointers */
    if (_fileFilters[i].filename == file_)
#else
    /* need to do a complete strcmp when not using fast lookup */
    if (strcmp(_fileFilters[i].filename, file_) == 0)
#endif
    {
      return (&_fileFilters[i]);
    }
  }
  return (NULL);
}

/******************************************************************************/
/******************************************************************************/
FunctionFilter *findFunctionFilter(const char *function_)
{
  for (int i = 0; i < _numFunctionFilters; i++)
  {
    if (strcmp(function_, _functionFilters[i].functionName) == 0)
    {
      return (&_functionFilters[i]);
    }
  }
  return (NULL);
}

/******************************************************************************/
/******************************************************************************/
ThreadFilter *findThreadFilter(const char *threadName_)
{
  for (int i = 0; i < _numThreadFilters; i++)
  {
    if (strcmp(threadName_, _threadFilters[i].threadName) == 0)
    {
      return (&_threadFilters[i]);
    }
  }
  return (NULL);
}

/******************************************************************************/
/******************************************************************************/
ThreadFilter *findThreadFilter(pthread_t threadId_)
{
  for (int i = 0; i < _numThreadFilters; i++)
  {
    if (_threadFilters[i].threadId == threadId_)
    {
      return (&_threadFilters[i]);
    }
  }
  return (NULL);
}

/******************************************************************************/
/******************************************************************************/
void removeFileFilter(const char *file_)
{
  FileFilter *filter;
  const char *symbol;
  if (((symbol = findSymbol(file_)) != NULL) && ((filter = findFileFilter(symbol)) != NULL))
  {
    _numFileFilters--;
#ifndef TF_FAST_FILENAME_LOOKUP
    free((void *)filter->filename);
#endif
    filter->filename = _fileFilters[_numFileFilters].filename;
    filter->level = _fileFilters[_numFileFilters].level;
    filter->numLineFilters = _fileFilters[_numFileFilters].numLineFilters;
    for (int i = 0; i < _fileFilters[_numFileFilters].numLineFilters; i++)
    {
      filter->lineFilters[i].minLine = _fileFilters[_numFileFilters].lineFilters[i].minLine;
      filter->lineFilters[i].maxLine = _fileFilters[_numFileFilters].lineFilters[i].maxLine;
    }
    if (_numFileFilters == 0)
    {
      _fileFilterEnabled = false;
      if ((_numFunctionFilters == 0) && (_numThreadFilters == 0))
      {
        _localFilterEnabled = false;
      }
    }
  }
}

/******************************************************************************/
/******************************************************************************/
void removeFunctionFilter(const char *function_)
{
  FunctionFilter *filter;
  if ((filter = findFunctionFilter(function_)) != NULL)
  {
    _numFunctionFilters--;
    free(filter->functionName);
    filter->functionName = _functionFilters[_numFunctionFilters].functionName;
    filter->level = _functionFilters[_numFunctionFilters].level;
    if (_numFunctionFilters == 0)
    {
      _functionFilterEnabled = false;
      if ((_numFileFilters == 0) && (_numThreadFilters == 0))
      {
        _localFilterEnabled = false;
      }
    }
  }
}

/******************************************************************************/
/******************************************************************************/
void removeThreadFilter(const char *thread_)
{
  RegisteredThread *thread;
  ThreadFilter *filter;
  if (((thread = findRegisteredThread(thread_)) != NULL) && ((filter = findThreadFilter(thread->threadName)) != NULL))
  {
    _numThreadFilters--;
    filter->threadName = _threadFilters[_numThreadFilters].threadName;
    filter->level = _threadFilters[_numThreadFilters].level;
    if (_numThreadFilters == 0)
    {
      _threadFilterEnabled = false;
      if ((_numFileFilters == 0) && (_numFunctionFilters == 0))
      {
        _localFilterEnabled = false;
      }
    }
  }
}

/******************************************************************************/
/******************************************************************************/
void removeAllFileFilters(void)
{
  _fileFilterEnabled = false;
  for (int i = 0; i < _numFileFilters; i++)
  {
#ifndef TF_FAST_FILENAME_LOOKUP
    free((void *)_fileFilters[i].filename);
#endif
  }
  _numFileFilters = 0;
  if ((_numFunctionFilters == 0) && (_numThreadFilters == 0))
  {
    _localFilterEnabled = false;
  }
}

/******************************************************************************/
/******************************************************************************/
void removeAllFunctionFilters(void)
{
  _functionFilterEnabled = false;
  for (int i = 0; i < _numFunctionFilters; i++)
  {
    free(_functionFilters[i].functionName);
  }
  _numFunctionFilters = 0;
  if ((_numFileFilters == 0) && (_numThreadFilters == 0))
  {
    _localFilterEnabled = false;
  }
}

/******************************************************************************/
/******************************************************************************/
void removeAllThreadFilters(void)
{
  _threadFilterEnabled = false;
  _numThreadFilters = 0;
  if ((_numFileFilters == 0) && (_numFunctionFilters == 0))
  {
    _localFilterEnabled = false;
  }
}

/******************************************************************************/
/******************************************************************************/
void tokenize(char *string_, Tokens &tokens_, const char *delimiter_)
{
  char *str;
  tokens_.numTokens = 0;
  if ((str = strtok(string_, delimiter_)) != NULL)
  {
    tokens_.tokens[tokens_.numTokens++] = str;
    while ((str = strtok(NULL, delimiter_)) != NULL)
    {
      if (tokens_.numTokens < TF_MAX_TOKENS)
      {
        tokens_.tokens[tokens_.numTokens++] = str;
      }
      else
      {
        break;
      }
    }
  }
}
