#!/usr/bin/python

#################################################################################
#
# Copyright (c) 2009, Ron Iovine, All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in the
#       documentation and/or other materials provided with the distribution.
#     * Neither the name of Ron Iovine nor the names of its contributors
#       may be used to endorse or promote products derived from this software
#       without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY Ron Iovine ''AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL Ron Iovine BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
#################################################################################

#################################################################################
#
# This is an example demo program that uses all the basic features of the PSHELL
# server python module.  This program can be run as either a UDP, TCP, UNIX, or
# local server based on the command line options.  If it is run as a UDP or UNIX
# based server, you must use the provided stand-alone UDP client program 'pshell'
# or 'pshell.py' to connect to it, if it is run as a TCP server, you must use
# 'telnet, if it is run as a local server, user command line input is solicited
# directly from this program, no external client is needed.
#
#################################################################################

# import all our necessary modules
import sys
import signal
import time
import PshellServer

#################################################################################
#
# PSHELL user callback functions, the interface is similar to the "main" in C
# and Python, with the argv argument being the argument list and the len and
# contents of argv are the user entered arguments, excluding the actual command
# itself (arguments only).
#
# Use the special 'PshellServer.printf' function call to display data back to
# the remote client.  The interface to this function is similar to the standard
# 'print' function in Python.
#
#################################################################################

#################################################################################
#################################################################################
def helloWorld(argv):
  PshellServer.printf("helloWorld command dispatched:")
  for index, arg in enumerate(argv):
    PshellServer.printf("  argv[%d]: '%s'" % (index, arg))

#################################################################################
#################################################################################
def enhancedUsage(argv):
  # see if the user asked for help
  if (PshellServer.isHelp()):
    # show standard usage
    PshellServer.showUsage()
    # give some enhanced usage
    PshellServer.printf("Enhanced usage here...")
  else:
    # do normal function processing
    PshellServer.printf("enhancedUsage command dispatched:")
    for index, arg in enumerate(argv):
      PshellServer.printf("  argv[%d]: '%s'" % (index, arg))

#################################################################################
#################################################################################
def formatChecking(argv):
  PshellServer.printf("formatChecking command dispatched:")
  if PshellServer.isIpv4Addr(argv[0]):
    PshellServer.printf("IPv4 address entered: '%s' entered" % argv[0])
  elif PshellServer.isIpv4AddrWithNetmask(argv[0]):
    PshellServer.printf("IPv4 address/netmask entered: '%s' entered" % argv[0])
  elif PshellServer.isDec(argv[0]):
    PshellServer.printf("Decimal arg: %d entered" % PshellServer.getInt(argv[0]))
  elif PshellServer.isHex(argv[0]):
    PshellServer.printf("Hex arg: 0x%x entered" % PshellServer.getInt(argv[0]))
  elif PshellServer.isAlpha(argv[0]):
    if PshellServer.isEqual(argv[0], "myarg"):
      PshellServer.printf("Alphabetic arg: '%s' equal to 'myarg'" % argv[0])
    else:
      PshellServer.printf("Alphabetic arg: '%s' not equal to 'myarg'" % argv[0])
  elif PshellServer.isAlphaNumeric(argv[0]):
    if PshellServer.isEqual(argv[0], "myarg1"):
      PshellServer.printf("Alpha numeric arg: '%s' equal to 'myarg1'" % argv[0])
    else:
      PshellServer.printf("Alpha numeric arg: '%s' not equal to 'myarg1'" % argv[0])
  elif PshellServer.isFloat(argv[0]):
    PshellServer.printf("Float arg: %.2f entered" % PshellServer.getFloat(argv[0]))
  else:
    PshellServer.printf("Unknown arg format: '%s'" % argv[0])

#################################################################################
#################################################################################
def wildcardMatch(argv):
  if (PshellServer.isHelp()):
    PshellServer.printf()
    PshellServer.showUsage()
    PshellServer.printf()
    PshellServer.printf("  where valid <args> are:")
    PshellServer.printf("    on")
    PshellServer.printf("    of*f")
    PshellServer.printf("    a*ll")
    PshellServer.printf("    sy*mbols")
    PshellServer.printf("    se*ttings")
    PshellServer.printf("    d*efault")
    PshellServer.printf()
  elif (PshellServer.isSubString(argv[0], "on", 2)):
    PshellServer.printf("argv 'on' match")
  elif (PshellServer.isSubString(argv[0], "off", 2)):
    PshellServer.printf("argv 'off' match")
  elif (PshellServer.isSubString(argv[0], "all", 1)):
    PshellServer.printf("argv 'all' match")
  elif (PshellServer.isSubString(argv[0], "symbols", 2)):
    PshellServer.printf("argv 'symbols' match")
  elif (PshellServer.isSubString(argv[0], "settings", 2)):
    PshellServer.printf("argv 'settings' match")
  elif (PshellServer.isSubString(argv[0], "default", 1)):
    PshellServer.printf("argv 'default' match")
  else:
    PshellServer.printf()
    PshellServer.showUsage()
    PshellServer.printf()
    PshellServer.printf("  where valid <args> are:")
    PshellServer.printf("    on")
    PshellServer.printf("    of*f")
    PshellServer.printf("    a*ll")
    PshellServer.printf("    sy*mbols")
    PshellServer.printf("    se*ttings")
    PshellServer.printf("    d*efault")
    PshellServer.printf()

#################################################################################
#################################################################################
def keepAlive(argv):
  if (PshellServer.isHelp()):
    PshellServer.printf()
    PshellServer.showUsage()
    PshellServer.printf()
    PshellServer.printf("Note, this function demonstrates intermediate flushes in a")
    PshellServer.printf("callback command to keep the UDP/UNIX interactive client from" )
    PshellServer.printf("timing out for commands that take longer than the response")
    PshellServer.printf("timeout (default=5 sec).  This is only supported in the 'C'")
    PshellServer.printf("version of the pshell interactive client, the Python version")
    PshellServer.printf("of the interactive client does not support intermediate flushes.")
    PshellServer.printf()
    return
  elif (argv[0] == "dots"):
    PshellServer.printf("marching dots keep alive:")
    for i in range(1,10):
      PshellServer.march(".")
      time.sleep(1)
  elif (argv[0] == "bang"):
    PshellServer.printf("marching 'bang' keep alive:")
    for i in range(1,10):
      PshellServer.march("!")
      time.sleep(1)
  elif (argv[0] == "pound"):
    PshellServer.printf("marching pound keep alive:")
    for i in range(1,10):
      PshellServer.march("#")
      time.sleep(1)
  elif (argv[0] == "wheel"):
    PshellServer.printf("spinning wheel keep alive:")
    for i in range(1,10):
      # string is optional, use NULL to omit
      PshellServer.wheel("optional string: ")
      time.sleep(1)
  else:
    PshellServer.showUsage()
    return
  PshellServer.printf()

# function to show advanced command line parsing using the PshellServer.tokenize function

MAX_YEAR   = 3000
MAX_MONTH  = 12
MAX_DAY    = 31
MAX_HOUR   = 23
MAX_MINUTE = 59
MAX_SECOND = 59

#################################################################################
#################################################################################
def advancedParsing(argv):

  numTokens, timestamp = PshellServer.tokenize(argv[0], ":")

  if (numTokens != 6):
    PshellServer.printf("ERROR: Improper timestamp format!!")
    PshellServer.showUsage()
  elif (not PshellServer.isDec(timestamp[0]) or
        PshellServer.getInt(timestamp[0]) > MAX_YEAR):
    PshellServer.printf("ERROR: Invalid year: %s, must be numeric value <= %d" %
                       (timestamp[0], MAX_YEAR))
  elif (not PshellServer.isDec(timestamp[1]) or
        PshellServer.getInt(timestamp[1]) > MAX_MONTH):
    PshellServer.printf("ERROR: Invalid month: %s, must be numeric value <= %d" %
                        (timestamp[1], MAX_MONTH))
  elif (not PshellServer.isDec(timestamp[2]) or
        PshellServer.getInt(timestamp[2]) > MAX_DAY):
    PshellServer.printf("ERROR: Invalid day: %s, must be numeric value <= %d" %
                        (timestamp[2], MAX_DAY))
  elif (not PshellServer.isDec(timestamp[3]) or
        PshellServer.getInt(timestamp[3]) > MAX_HOUR):
    PshellServer.printf("ERROR: Invalid hour: %s, must be numeric value <= %d" %
                        (timestamp[3], MAX_HOUR))
  elif (not PshellServer.isDec(timestamp[4]) or
        PshellServer.getInt(timestamp[4]) > MAX_MINUTE):
    PshellServer.printf("ERROR: Invalid minute: %s, must be numeric value <= %d" %
                        (timestamp[4], MAX_MINUTE))
  elif (not PshellServer.isDec(timestamp[5]) or
        PshellServer.getInt(timestamp[5]) > MAX_SECOND):
    PshellServer.printf("ERROR: Invalid second: %s, must be numeric value <= %d" %
                        (timestamp[5], MAX_SECOND))
  else:
    PshellServer.printf("Year   : %s" % timestamp[0])
    PshellServer.printf("Month  : %s" % timestamp[1])
    PshellServer.printf("Day    : %s" % timestamp[2])
    PshellServer.printf("Hour   : %s" % timestamp[3])
    PshellServer.printf("Minute : %s" % timestamp[4])
    PshellServer.printf("Second : %s" % timestamp[5])

#################################################################################
#################################################################################
def getOptions(argv):
  if (PshellServer.isHelp()):
    PshellServer.printf()
    PshellServer.showUsage()
    PshellServer.printf()
    PshellServer.printf("  where:")
    PshellServer.printf("    arg - agrument of the format -<key><value> or <key>=<value>")
    PshellServer.printf()
    PshellServer.printf("  For the first form, the <key> must be a single character, e.g. -t10")
    PshellServer.printf("  for the second form, <key> can be any length, e.g. timeout=10")
    PshellServer.printf()
  else:
    for index, arg in enumerate(argv):
      (parsed, key, value) = PshellServer.getOption(arg)
      PshellServer.printf("arg[%d]: '%s', parsed: %s, key: '%s', value: '%s'" % (index, arg, parsed, key, value))

#################################################################################
#################################################################################
def showUsage():
  print("")
  print("Usage: pshellServerDemo.py -udp [<port>] | -tcp [<port>] | -unix | -local")
  print("")
  print("  where:")
  print("    -udp   - Multi-session UDP server")
  print("    -tcp   - Single session TCP server")
  print("    -unix  - Multi-session UNIX domain server")
  print("    -local - Local command dispatching server")
  print("    <port> - Desired UDP or TCP port, default: %d" % PSHELL_DEMO_PORT)
  print("")
  sys.exit()

#################################################################################
#################################################################################
def signalHandler(signal, frame):
  PshellServer.cleanupResources()
  print("")
  sys.exit()

#################################################################################
#################################################################################
def registerSignalHandlers():
  # register a signal handlers so we can cleanup our
  # system resources upon abnormal termination
  signal.signal(signal.SIGHUP, signalHandler)      # 1  Hangup (POSIX)
  signal.signal(signal.SIGINT, signalHandler)      # 2  Interrupt (ANSI)
  signal.signal(signal.SIGQUIT, signalHandler)     # 3  Quit (POSIX)
  signal.signal(signal.SIGILL, signalHandler)      # 4  Illegal instruction (ANSI)
  signal.signal(signal.SIGABRT, signalHandler)     # 6  Abort (ANSI)
  signal.signal(signal.SIGBUS, signalHandler)      # 7  BUS error (4.2 BSD)
  signal.signal(signal.SIGFPE, signalHandler)      # 8  Floating-point exception (ANSI)
  signal.signal(signal.SIGSEGV, signalHandler)     # 11 Segmentation violation (ANSI)
  signal.signal(signal.SIGPIPE, signalHandler)     # 13 Broken pipe (POSIX)
  signal.signal(signal.SIGALRM, signalHandler)     # 14 Alarm clock (POSIX)
  signal.signal(signal.SIGTERM, signalHandler)     # 15 Termination (ANSI)
  signal.signal(signal.SIGXCPU, signalHandler)     # 24 CPU limit exceeded (4.2 BSD)
  signal.signal(signal.SIGXFSZ, signalHandler)     # 25 File size limit exceeded (4.2 BSD)
  signal.signal(signal.SIGSYS, signalHandler)      # 31 Bad system call

##############################
#
# start of main program
#
##############################
if (__name__ == '__main__'):

  PSHELL_DEMO_PORT = 9001
  if (len(sys.argv) == 2 or len(sys.argv) == 3):
    if (sys.argv[1] == "-udp"):
      serverType = PshellServer.UDP
    elif (sys.argv[1] == "-unix"):
      serverType = PshellServer.UNIX
    elif (sys.argv[1] == "-tcp"):
      serverType = PshellServer.TCP
    elif (sys.argv[1] == "-local"):
      serverType = PshellServer.LOCAL
    else:
      showUsage()
    if len(sys.argv) == 3:
      PSHELL_DEMO_PORT = int(sys.argv[2])
  else:
    showUsage()

  # register signal handlers so we can do a graceful termination and cleanup any system resources
  registerSignalHandlers()

  # register our callback commands, commands consist of single keyword only
  PshellServer.addCommand(function    = helloWorld,
                          command     = "helloWorld",
                          description = "command that prints out arguments",
                          usage       = "[<arg1> ... <arg20>]",
                          minArgs     = 0,
                          maxArgs     = 20)

  # TCP or LOCAL servers don't need a keep-alive, so only add
  # this command for connectionless datagram type servers
  if ((serverType == PshellServer.UDP) or (serverType == PshellServer.UNIX)):
    PshellServer.addCommand(function    = keepAlive,
                            command     = "keepAlive",
                            description = "command to show client keep-alive ('C' client only)",
                            usage       = "dots | bang | pound | wheel",
                            minArgs     = 1,
                            showUsage   = False)

  PshellServer.addCommand(function    = wildcardMatch,
                          command     = "wildcardMatch",
                          description = "command that does a wildcard matching",
                          usage       = "<arg>",
                          minArgs     = 1,
                          showUsage   = False)

  PshellServer.addCommand(function    = enhancedUsage,
                          command     = "enhancedUsage",
                          description = "command with enhanced usage",
                          usage       = "<arg1>",
                          minArgs     = 1,
                          showUsage   = False)

  PshellServer.addCommand(function    = formatChecking,
                          command     = "formatChecking",
                          description = "command with arg format checking",
                          usage       = "<arg1>",
                          minArgs     = 1)

  PshellServer.addCommand(function    = advancedParsing,
                          command     = "advancedParsing",
                          description = "command with advanced command line parsing",
                          usage       = "<yyyy>:<mm>:<dd>:<hh>:<mm>:<ss>",
                          minArgs     = 1)

  PshellServer.addCommand(function    = getOptions,
                          command     = "getOptions",
                          description = "example of parsing command line options",
                          usage       = "<arg1> [<arg2>...<argN>]",
                          minArgs     = 1,
                          maxArgs     = 20,
                          showUsage   = False)

  # run a registered command from within it's parent process, this can be done before
  # or after the server is started, as long as the command being called is regstered
  PshellServer.runCommand("helloWorld 1 2 3")

  # Now start our PSHELL server with the PshellServer.startServer function call.
  #
  # The 1st argument is our serverName (i.e. "pshellServerDemo").
  #
  # The 2nd argument specifies the type of PSHELL server, the four valid values are:
  #
  #   PshellServer.UDP   - Server runs as a multi-session UDP based server.  This requires
  #                        the special stand-alone command line UDP/UNIX client program
  #                        'pshell'.  This server has no timeout for idle client sessions.
  #                        It can be also be controlled programatically via an external
  #                        program running the PshellControl API and library.
  #   PshellServer.UNIX  - Server runs as a multi-session UNIX based server.  This requires
  #                        the special stand-alone command line UDP/UNIX client program
  #                        'pshell'.  This server has no timeout for idle client sessions.
  #                        It can be also be controlled programatically via an external
  #                        program running the PshellControl API and library.
  #   PshellServer.TCP   - Server runs as a single session TCP based server with a 10 minute
  #                        idle client session timeout.  The TCP server can be connected to
  #                        using a standard 'telnet' based client.
  #   PshellServer.LOCAL - Server solicits it's own command input via the system command line
  #                        shell, there is no access via a separate client program, when the
  #                        user input is terminated via the 'quit' command, the program is
  #                        exited.
  #
  # The 3rd argument is the server mode, the two valid values are:
  #
  #   PshellServer.NON_BLOCKING - A separate thread will be created to process user input, when
  #                               this function is called as non-blocking, the function will return
  #                               control to the calling context.
  #   PshellServer.BLOCKING     - No thread is created, all processing of user input is done within
  #                               this function call, it will never return control to the calling context.
  #
  # The 4th and 5th arguments must be provided for a UDP or TCP server, for a LOCAL or
  # UNIX server they can be omitted, and if provided they will be ignored.
  #
  # For the 4th argument, a valid IP address or hostname can be used.  There are also 3 special
  # "hostname" type identifiers defined as follows:
  #
  #   localhost - the loopback address (i.e. 127.0.0.1)
  #   myhost    - the hostname assigned to this host, this is usually defined in the
  #               /etc/hosts file and is assigned to the default interface
  #   anyhost   - all interfaces on a multi-homed host (including loopback)
  #
  # Finally, the 5th argument is the desired port number.
  #
  # All of these arguments (except the server name and mode, i.e. args 1 & 3) can be overridden
  # via the 'pshell-server.conf' file on a per-server basis.

  #
  # NOTE: This server is started up in BLOCKING mode, but if a pshell server is being added
  #       to an existing process that already has a control loop in it's main, it should be
  #       started in NON_BLOCKING mode before the main drops into it's control loop, see the
  #       demo program traceFilterDemo.cc for an example
  #
  PshellServer.startServer("pshellServerDemo", serverType, PshellServer.BLOCKING, PshellServer.ANYHOST, PSHELL_DEMO_PORT)

  #
  # should never get here because the server is started in BLOCKING mode,
  # but cleanup any pshell system resources as good practice
  #
  PshellServer.cleanupResources()
