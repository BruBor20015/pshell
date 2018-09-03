/////////////////////////////////////////////////////////////////////////////////
// 
// Copyright (c) 2009, Ron Iovine, All rights reserved.  
//  
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of Ron Iovine nor the names of its contributors 
//       may be used to endorse or promote products derived from this software 
//       without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY Ron Iovine ''AS IS'' AND ANY EXPRESS OR 
// IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES 
// OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 
// IN NO EVENT SHALL Ron Iovine BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
// BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
// IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
// POSSIBILITY OF SUCH DAMAGE. 
//
/////////////////////////////////////////////////////////////////////////////////

package PshellServer

import "encoding/binary"
import "net"
import "fmt"
import "strings"
import "strconv"
import "io/ioutil"
import "os"
import "math"
import "bufio"
import "time"

/////////////////////////////////////////////////////////////////////////////////
//
// This API provides the Process Specific Embedded Command Line Shell (PSHELL)
// user API functionality.  It provides the ability for a client program to
// register functions that can be invoked via a command line user interface.
// This is the 'go' implementation, there are also corresponding C/C++ and
// Python implementations.
//
// The functions are similar to the prototype of the 'main' in 'go', i.e.
// 'func myFunc([]string), there are several ways to invoke these embedded
// functions based on how the pshell server is configured, which is described
// in documentation further down in this file.
//
// A complete example of the usage of the API can be found in the included 
// demo program file pshellServerDemo.go
//
/////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////
//
// global "public" data, these are used for various parts of the public API
//
/////////////////////////////////////////////////////////////////////////////////

// Valid server types, UDP/UNIX servers require the 'pshell' or 'pshell.py'
// client programs, TCP servers require a 'telnet' client, local servers
// require no client (all user interaction done directly with server running
// in the parent host program)
const (
  UDP = "udp"
  TCP = "tcp"
  UNIX = "unix"
  LOCAL = "local"
)

// These are the identifiers for the serverMode.  BLOCKING wil never return 
// control to the caller of startServer, NON_BLOCKING will spawn a thread to 
// run the server and will return control to the caller of startServer
const (
  BLOCKING = 0
  NON_BLOCKING = 1
)

// These three identifiers that can be used for the hostnameOrIpAddr argument 
// of the startServer call.  PshellServer.ANYHOST will bind the server socket
// to all interfaces of a multi-homed host, PSHELL_ANYBCAST will bind to
// 255.255.255.255, PshellServer.LOCALHOST will bind the server socket to 
// the local loopback address (i.e. 127.0.0.1), note that subnet broadcast 
// it also supported, e.g. x.y.z.255
const (
  ANYHOST = "anyhost"
  ANYBCAST = "anybcast"
  LOCALHOST = "localhost"
)

/////////////////////////////////////////////////////////////////////////////////
//
// global "private" data
//
/////////////////////////////////////////////////////////////////////////////////

// the following enum values are returned by the pshell server
// response to a command request from a control client
const (
  _COMMAND_SUCCESS = 0
  _COMMAND_NOT_FOUND = 1
  _COMMAND_INVALID_ARG_COUNT = 2
)

// msgType codes between interactive client and server
const (
  _QUERY_VERSION = 1
  _QUERY_PAYLOAD_SIZE = 2
  _QUERY_NAME = 3
  _QUERY_COMMANDS1 = 4
  _QUERY_COMMANDS2 = 5
  _UPDATE_PAYLOAD_SIZE = 6
  _USER_COMMAND = 7
  _COMMAND_COMPLETE = 8
  _QUERY_BANNER = 9
  _QUERY_TITLE = 10
  _QUERY_PROMPT = 11
  _CONTROL_COMMAND = 12
)

// ascii keystroke codes
const (
  _BS = 8
  _TAB = 9
  _LF = 10
  _CR = 13
  _ESC = 27
  _SPACE = 32
  _DEL = 127
)

type pshellFunction func([]string)

type pshellCmd struct {
  command string
  usage string
  description string
  function pshellFunction
  minArgs int
  maxArgs int
  showUsage bool
}

const _PSHELL_CONFIG_DIR = "/etc/pshell/config"
const _PSHELL_STARTUP_DIR = "/etc/pshell/startup"
const _PSHELL_BATCH_DIR = "/etc/pshell/batch"
const _PSHELL_CONFIG_FILE = "pshell-server.conf"

var _gPrompt = "PSHELL> "
var _gTitle = "PSHELL"
var _gTcpTitle = "PSHELL"
var _gTcpPrompt = ""
var _gQuitTcp = false
var _gBanner = "PSHELL: Process Specific Embedded Command Line Shell"
var _gServerVersion = "1"
var _gPshellMsgPayloadLength = 2048
var _gPshellMsgHeaderLength = 8
var _gServerType = UDP
var _gServerName = "None"
var _gServerMode = BLOCKING
var _gHostnameOrIpAddr = "None"
var _gPort = "0"
var _gTcpTimeout = 10  // minutes
var _gTcpConnectSockName = ""
var _gRunning = false
var _gCommandDispatched = false
var _gCommandInteractive = true
var _gArgs []string
var _gFoundCommand pshellCmd

var _gCommandList = []pshellCmd{}
var _gPshellRcvMsg = make([]byte, _gPshellMsgPayloadLength)
var _gPshellSendPayload = ""
var _gUdpSocket *net.UDPConn
var _gUnixSocket *net.UnixConn
var _gUnixSocketPath = "/tmp/"
var _gUnixSourceAddress string
var _gTcpSocket *net.TCPListener
var _gConnectFd *net.TCPConn
var _gTcpNegotiate = []byte{0xFF, 0xFB, 0x03, 0xFF, 0xFB, 0x01, 0xFF, 0xFD, 0x03, 0xFF, 0xFD, 0x01}
var _gRecvAddr net.Addr
var _gMaxLength = 0
var _gWheelPos = 0
var _gWheel = "|/-\\"

var _gTabCompletions []string
var _gMaxTabCompletionKeywordLength = 0
var _gMaxCompletionsPerLine = 0
var _gCommandHistory []string
var _gCommandHistoryPos = 0

/////////////////////////////////
//
// Public functions
//
/////////////////////////////////

//
//  Register callback commands to our PSHELL server.  If the command takes no
//  arguments, the default parameters can be provided.  If the command takes
//  an exact number of parameters, set minArgs and maxArgs to be the same.  If
//  the user wants the callback function to handle all help initiated usage,
//  set the showUsage parameter to false.
//
//    Args:
//        function (ptr)    : User callback function
//        command (str)     : Command to dispatch the function (single keyword only)
//        description (str) : One line description of command
//        usage (str)       : One line command usage (Unix style preferred)
//        minArgs (int)     : Minimum number of required arguments
//        maxArgs (int)     : Maximum number of required arguments
//        showUsage (bool)  : Show registered usage on a '?' or '-h'
//
//    Returns:
//        none
//
func AddCommand(function pshellFunction,
                command string,
                description string,
                usage string,
                minArgs int,
                maxArgs int,
                showUsage bool) {
  addCommand(function,
             command,
             description,
             usage,
             minArgs,
             maxArgs,
             showUsage,
             false)
}

//
//  Start our PSHELL server, if serverType is UNIX or LOCAL, the default
//  parameters can be used, and will be ignored if provided.  All of these
//  parameters except serverMode can be overridden on a per serverName
//  basis via the pshell-server.conf config file.  All commands in the
//  <serverName>.startup file will be executed in this function at server
//  startup time.
//
//    Args:
//        serverName (str)       : Logical name of the Pshell server
//        serverType (str)       : Desired server type (UDP, UNIX, TCP, LOCAL)
//        serverMode (str)       : Desired server mode (BLOCKING, NON_BLOCKING)
//        hostnameOrIpAddr (str) : Hostname or IP address to run server on
//        port (int)             : Port number to run server on (UDP or TCP only)
//
//    Returns:
//        none
//
func StartServer(serverName string,
                 serverType string,
                 serverMode int,
                 hostnameOrIpAddr string,
                 port string) {
  startServer(serverName,
              serverType,
              serverMode,
              hostnameOrIpAddr,
              port)
}

//
//  Cleanup and release any system resources claimed by this module.  This includes
//  any open socked handles, file descriptors, or system 'tmp' files.  This should
//  be called at program exit time as well as any signal exception handler that
//  results in a program termination.
//
//    Args:
//        none
//
//    Returns:
//        none
//
func CleanupResources() {
  cleanupResources()
}

//
//  Run a registered command from within its parent process
//
//    Args:
//        command (str) : Registered callback command to run
//
//    Returns:
//        none
//
func RunCommand(format string, command ...interface{}) {
  runCommand(format, command...)
}

////////////////////////////////////////////////////////////////////////////////
//
// The following public functions should only be called from within a 
// registered callback function
//
////////////////////////////////////////////////////////////////////////////////

//
//  Display data back to the remote client
//
//    Args:
//        string (str)   : Message to display to the client user
//
//    Returns:
//        none
//
func Printf(format string, message ...interface{}) {
  printf(format, message...)
}

//
//  Flush the reply (i.e. display) buffer back to the remote client
//
//    Args:
//        none
//
//    Returns:
//        none
//
func Flush() {
  flush()
}

//
//  Spinning ascii wheel keep alive, user string string is optional
//
//    Args:
//        message (str) : String to display before the spinning wheel
//
//    Returns:
//        none
//
func Wheel(message string) {
  wheel(message)
}

//
//  March a message or character keep alive across the screen
//
//    Args:
//        message (str) : String or char to march across the screen
//
//    Returns:
//        none
//
func March(message string) {
  march(message)
}

//
//  Check if the user has asked for help on this command.  Command must be 
//  registered with the showUsage = false option.
//
//    Args:
//        none
//
//    Returns:
//        bool : True if user is requesting command help, false otherwise
//
func IsHelp() bool {
  return (isHelp())
}

//
//  Show the command's registered usage
//
//    Args:
//        none
//
//    Returns:
//        none
//
func ShowUsage() {
  showUsage()
}

//
//  This function will return True if string1 is a substring of string2 at 
//  position 0.  If the minMatchLength is 0, then it will compare up to the
//  length of string1.  If the minMatchLength > 0, it will require a minimum
//  of that many characters to match.  A string that is longer than the min
//  match length must still match for the remaining charactes, e.g. with a
//  minMatchLength of 2, 'q' will not match 'quit', but 'qu', 'qui' or 'quit'
//  will match, 'quix' will not match.  This function is useful for wildcard
//  matching.
//
//    Args:
//        string1 (str)        : The substring
//        string2 (str)        : The target string
//        minMatchLength (int) : The minimum required characters match
//
//    Returns:
//        bool : True if substring matches, false otherwise
//
func IsSubString(string1 string, string2 string, minMatchLength int) bool {
  return isSubString(string1, string2, minMatchLength)
}

//
//  This function will parse an argument string of the formats -<key><value> where
//  key is one letter only, i.e. '-t', or <key>=<value> where key can be any length
//  word, i.e. 'timeout', and return a 3-tuple indicating if the arg was parsed
//  correctly, along with the associated key and corresponding value.  An example 
//  of the two valid formats are -t10, timeout=10.
//
//  Args:
//        arg (str) : The argument string to parse
//
//    Returns:
//        bool : True if string parses correctly, i.e. -<key><value> or <key>=<value>
//        str  : The key value found
//        str  : The value associated with the key
//
func GetOption(arg string) (bool, string, string) {
  return (getOption(arg))
}

/////////////////////////////////
//
// Private functions
//
/////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func addCommand(function_ pshellFunction, 
                command_ string, 
                description_ string, 
                usage_ string, 
                minArgs_ int, 
                maxArgs_ int, 
                showUsage_ bool,
                prepend_ bool) {  
                  
  // see if we have a NULL command name 
  if ((command_ == "") || (len(command_) == 0)) {
    fmt.Printf("PSHELL_ERROR: NULL command name, command not added\n")
    return
  }

  // see if we have a NULL description 
  if ((description_ == "") || (len(description_) == 0)) {
    fmt.Printf("PSHELL_ERROR: NULL description, command: '%s' not added\n", command_)
    return
  }

  // see if we have a NULL function
  if (function_ == nil) {
    fmt.Printf("PSHELL_ERROR: NULL function, command: '%s' not added\n", command_)
    return
  }

  // if they provided no usage for a function with arguments
  if (((maxArgs_ > 0) || (minArgs_ > 0)) && ((usage_ == "") || (len(usage_) == 0))) {
    fmt.Printf("PSHELL_ERROR: NULL usage for command that takes arguments, command: '%s' not added\n", command_)
    return
  }

  // see if their minArgs is greater than their maxArgs
  if (minArgs_ > maxArgs_) {
    fmt.Printf("PSHELL_ERROR: minArgs: %d is greater than maxArgs: %d, command: '%s' not added\n", minArgs_, maxArgs_, command_)
    return
  }
    
  // see if it is a duplicate command
  for _, entry := range _gCommandList {
    if (entry.command == command_) {
      // command name already exists, don't add it again
      fmt.Printf("PSHELL_ERROR: Command: %s already exists, not adding command\n", command_)
      return
    }
  }
      
  // everything ok, good to add command
  
  if (len(command_) > _gMaxLength) {
    _gMaxLength = len(command_)
  }
    
  if (prepend_ == true) {
    _gCommandList = append([]pshellCmd{{command_,
                                        usage_,
                                        description_,
                                        function_,
                                        minArgs_,
                                        maxArgs_,
                                        showUsage_}},
                           _gCommandList...)
  } else {
    _gCommandList = append(_gCommandList, 
                           pshellCmd{command_, 
                                     usage_,
                                     description_, 
                                     function_,
                                     minArgs_, 
                                     maxArgs_,
                                     showUsage_})
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func startServer(serverName_ string,
                 serverType_ string,
                 serverMode_ int,
                 hostnameOrIpAddr_ string, 
                 port_ string) {
  if (_gRunning == false) {
    _gServerName = serverName_
    _gServerType = serverType_
    _gServerMode = serverMode_
    _gHostnameOrIpAddr = hostnameOrIpAddr_
    _gPort = port_
    loadConfigFile()
    loadStartupFile()  
    _gRunning = true
    if (_gServerMode == BLOCKING) {
      runServer()
    } else {
      go runServer()
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func cleanupResources() {
  if _gServerType == UNIX {
    os.Remove(_gUnixSourceAddress)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runCommand(format_ string, command_ ...interface{}) {
  if (_gCommandDispatched == false) {
    command := fmt.Sprintf(format_, command_...)
    _gCommandDispatched = true
    _gCommandInteractive = false
    numMatches := 0
    _gArgs = strings.Split(strings.TrimSpace(command), " ")
    command = _gArgs[0]
    if (len(_gArgs) > 1) {
      _gArgs = _gArgs[1:]
    } else {
      _gArgs = []string{}
    }
    for _, entry := range _gCommandList {
      if (command == entry.command) {
        _gFoundCommand = entry
        numMatches += 1
      }
    }
    if ((numMatches == 1) && isValidArgCount() && !IsHelp()) {
      _gFoundCommand.function(_gArgs)
    }
    _gCommandDispatched = false
    _gCommandInteractive = true
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func printf(format_ string, message_ ...interface{}) {
  if (_gCommandInteractive == true) {
    if (_gServerType == LOCAL) {
      fmt.Printf(format_, message_...)
    } else {
      // UDP/TCP/Unix server
      _gPshellSendPayload += fmt.Sprintf(format_, message_...)
      if (_gServerType == TCP) {
        flush()
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func isHelp() bool {
  return ((len(_gArgs) == 1) &&
          ((_gArgs[0] == "?") ||
           (_gArgs[0] == "-h") ||
           (_gArgs[0] == "--h") ||
           (_gArgs[0] == "-help") ||
           (_gArgs[0] == "--help")))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func flush() {
  if (_gCommandInteractive == true) {
    if ((_gServerType == UDP) || (_gServerType == UNIX)) {
      reply(getMsgType(_gPshellRcvMsg))
    } else if (_gServerType == TCP) {
      _gConnectFd.Write([]byte(strings.Replace(_gPshellSendPayload, "\n", "\r\n", -1)))
      _gPshellSendPayload = ""
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func wheel(message_ string) {
  _gWheelPos += 1
  if (message_ != "") {
    printf("\r%s%c", message_, _gWheel[(_gWheelPos)%4])
  } else {
    printf("\r%c", _gWheel[(_gWheelPos)%4])
  }
  flush()
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func march(message_ string) {
  printf(message_)
  flush()
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func showUsage() {
  if (len(_gFoundCommand.usage) > 0) {
    printf("Usage: %s %s\n", _gFoundCommand.command, _gFoundCommand.usage)
  } else {
    printf("Usage: %s\n", _gFoundCommand.command)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func isSubString(string1_ string, string2_ string, minMatchLength_ int) bool {
  if (minMatchLength_ > len(string1_)) {
    return false
  } else {
    return strings.HasPrefix(string2_, string1_)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getOption(arg_ string) (bool, string, string) {
  if (len(arg_) < 3) {
    return false, "", ""
  } else if (arg_[0] == '-') {
    return true, arg_[:2], arg_[2:]
  } else {
    value := strings.Split(arg_, "=")
    if (len(value) != 2) {
      return false, "", ""
    } else {
      return true, value[0], value[1]
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func loadConfigFile() {
  var configFile1 = ""
  var file []byte
  configPath := os.Getenv("PSHELL_CONFIG_DIR")
  if (configPath != "") {
    configFile1 = configPath+"/"+_PSHELL_CONFIG_FILE
  }
  configFile2 := _PSHELL_CONFIG_DIR+"/"+_PSHELL_CONFIG_FILE
  cwd, _ := os.Getwd()
  configFile3 := cwd+"/"+_PSHELL_CONFIG_FILE
  if _, err := os.Stat(configFile1); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(configFile1)
  } else if _, err := os.Stat(configFile2); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(configFile2)
  } else if _, err := os.Stat(configFile3); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(configFile3)
  } else {
    return
  }
  // found a config file, process it
  lines := strings.Split(string(file), "\n")
  for _, line := range lines {
    // skip comments
    if ((len(line) > 0) && (line[0] != '#')) {
      value := strings.Split(line, "=")
      if (len(value) == 2) {
        option := strings.Split(value[0], ".")
        if ((len(option) == 2) && (_gServerName == option[0])) {
          if (strings.ToLower(option[1]) == "title") {
            _gTitle = value[1]
          } else if (strings.ToLower(option[1]) == "banner") {
            _gBanner = value[1]
          } else if (strings.ToLower(option[1]) == "prompt") {
            _gPrompt = value[1]
          } else if (strings.ToLower(option[1]) == "host") {
            _gHostnameOrIpAddr = value[1]
          } else if (strings.ToLower(option[1]) == "port") {
            _gPort = value[1]
          } else if (strings.ToLower(option[1]) == "type") {
            if ((strings.ToLower(value[1]) == UDP) ||
                (strings.ToLower(value[1]) == TCP) ||
                (strings.ToLower(value[1]) == UNIX) ||
                (strings.ToLower(value[1]) == LOCAL)) {
              _gServerType = value[1]
            }
          } else if (strings.ToLower(option[1]) == "timeout") {
            _gTcpTimeout, _ = strconv.Atoi(value[1])
          }
        }
      }
    }
  }
  return
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func showWelcome() {
  var server = ""
  // show our welcome screen
  banner := "#  " + _gBanner + "\n"
  // put up our window title banner
  if (_gServerType == LOCAL) {
    printf("\033]0;%s\007", _gTitle)
    server = fmt.Sprintf("#  Single session LOCAL server: %s[%s]\n",
                         _gServerName,
                         _gServerType)
  } else {
    printf("\033]0;%s\007", _gTcpTitle)
    server = fmt.Sprintf("#  Single session TCP server: %s[%s]\n",
                         _gServerName,
                         _gTcpConnectSockName)
  }
  maxBorderWidth := math.Max(58, float64(len(banner)-1))
  maxBorderWidth = math.Max(maxBorderWidth, float64(len(server)))+2
  printf("\n")
  printf(strings.Repeat("#", int(maxBorderWidth)))
  printf("\n")
  printf("#\n")
  printf(banner)
  printf("#\n")
  printf(server)
  printf("#\n")
  if (_gServerType == LOCAL) {
    printf("#  Idle session timeout: NONE\n")
  } else {
    printf("#  Idle session timeout: %d minutes\n", _gTcpTimeout)
  }
  printf("#\n")
  printf("#  Type '?' or 'help' at prompt for command summary\n")
  printf("#  Type '?' or '-h' after command for command usage\n")
  printf("#\n")
  if (_gServerType == TCP) {
    printf("#  Full <TAB> completion, up-arrow recall, command\n")
    printf("#  line editing and command abbreviation supported\n")
  } else {
    printf("#  Command abbreviation supported\n")
  }
  printf("#\n")
  printf(strings.Repeat("#", int(maxBorderWidth)))
  printf("\n")
  printf("\n")
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func loadStartupFile() {
  var startupFile1 = ""
  var file []byte
  startupPath := os.Getenv("PSHELL_STARTUP_DIR")
  if (startupPath != "") {
    startupFile1 = startupPath+"/"+_gServerName+".startup"
  }
  startupFile2 := _PSHELL_STARTUP_DIR+"/"+_gServerName+".startup"
  cwd, _ := os.Getwd()
  startupFile3 := cwd+"/"+_gServerName+".startup"
  if _, err := os.Stat(startupFile1); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(startupFile1)
  } else if _, err := os.Stat(startupFile2); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(startupFile2)
  } else if _, err := os.Stat(startupFile3); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(startupFile3)
  } else {
    // file not found, return
    return
  }
  // found a startup file, process it
  lines := strings.Split(string(file), "\n")
  for _, line := range lines {
    // skip comments
    if ((len(line) > 0) && (line[0] != '#')) {
      runCommand(line)
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func batch(argv_ []string) {
  var batchFile = argv_[0]
  var batchFile1 = ""
  var file []byte
  batchPath := os.Getenv("PSHELL_BATCH_DIR")
  if (batchPath != "") {
    batchFile1 = batchPath+"/"+batchFile
  }
  batchFile2 := _PSHELL_BATCH_DIR+"/"+batchFile
  cwd, _ := os.Getwd()
  batchFile3 := cwd+"/"+batchFile
  batchFile4 := batchFile
  if _, err := os.Stat(batchFile1); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(batchFile1)
  } else if _, err := os.Stat(batchFile2); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(batchFile2)
  } else if _, err := os.Stat(batchFile3); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(batchFile3)
  } else if _, err := os.Stat(batchFile4); !os.IsNotExist(err) {
    file, _ = ioutil.ReadFile(batchFile4)
  } else {
    // file not found, return
    printf("ERROR: Could not find batch file: '%s'\n", batchFile)
    return
  }
  // found a batch file, process it
  lines := strings.Split(string(file), "\n")
  for _, line := range lines {
    // skip comments
    if ((len(line) > 0) && (line[0] != '#')) {
      processCommand(line)
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func help(argv_ []string) {
  printf("\n")
  printf("****************************************\n")
  printf("*             COMMAND LIST             *\n")
  printf("****************************************\n")
  printf("\n")
  processQueryCommands1()
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func exit(argv_ []string) {
  if (_gServerType == LOCAL) {
    //local server, exit the process
    os.Exit(0)
  } else if (_gServerType == TCP) {
    _gQuitTcp = true
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runServer() {
  if (_gServerType == UDP) {
    runUDPServer()
  } else if (_gServerType == TCP) {
    runTCPServer()
  } else if (_gServerType == UNIX) {
    runUNIXServer()
  } else {  // local server 
    runLocalServer()
  }
}
 
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runUDPServer() {
  fmt.Printf("PSHELL_INFO: UDP Server: %s Started On Host: %s, Port: %s\n",
             _gServerName,
             _gHostnameOrIpAddr,
             _gPort)
  runDGRAMServer()
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runUNIXServer() {
  fmt.Printf("PSHELL_INFO: UNIX Server: %s Started\n", _gServerName)
  runDGRAMServer()
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runDGRAMServer() {
  addNativeCommands()
  if (createSocket()) {
    for {
      receiveDGRAM()
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func addNativeCommands() {
  addCommand(batch,
             "batch",
             "run commands from a batch file",
             "<filename>",
             1,
             2,
             true,
             true)
  if ((_gServerType == TCP) || (_gServerType == LOCAL)) {
    addCommand(help,
               "help",
               "show all available commands",
               "",
               0,
               0,
               true,
               true)
    addCommand(exit,
               "quit",
               "exit interactive mode",
               "",
               0,
               0,
               true,
               true)
  }
  addTabCompletions()
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runLocalServer() {
  _gPrompt = _gServerName + "[" + _gServerType + "]:" + _gPrompt
  _gTitle = _gTitle + ": " + _gServerName + "[" +
            _gServerType + "], Mode: INTERACTIVE"
  addNativeCommands()
  showWelcome()
  reader := bufio.NewReader(os.Stdin)
  for {
    fmt.Print(_gPrompt)
    command, _ := reader.ReadString('\n')
    command = strings.TrimSuffix(command, "\n")
    if (len(command) > 0) {
      processCommand(command)
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func acceptConnection() bool {
  _gConnectFd, _ = _gTcpSocket.AcceptTCP()
  _gTcpConnectSockName = strings.Split(_gConnectFd.LocalAddr().String(), ":")[0]
   return (true)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runTCPServer() {
  fmt.Printf("PSHELL_INFO: TCP Server: %s Started On Host: %s, Port: %s\n",
             _gServerName,
             _gHostnameOrIpAddr,
             _gPort)
  _gTcpPrompt = _gServerName + "[" + _gTcpConnectSockName + "]:" + _gPrompt
  _gTcpTitle = _gTitle + ": " + _gServerName + "[" +
               _gTcpConnectSockName + "], Mode: INTERACTIVE"
  addNativeCommands()
  // startup our TCP server and accept new connections
  for createSocket() && acceptConnection() {
    _gTcpPrompt = _gServerName + "[" + _gTcpConnectSockName + "]:" + _gPrompt
    _gTcpTitle = _gTitle + ": " + _gServerName + "[" +
                 _gTcpConnectSockName + "], Mode: INTERACTIVE"
    // shutdown original socket to not allow any new connections
    // until we are done with this one
    _gTcpSocket.Close()
    receiveTCP()
    _gConnectFd.Close()
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func createSocket() bool {
  var hostnameOrIpAddr = ""
  if (_gHostnameOrIpAddr == ANYHOST) {
    hostnameOrIpAddr = ""
  } else if (_gHostnameOrIpAddr == LOCALHOST) {
    hostnameOrIpAddr = "127.0.0.1"
  } else {
    hostnameOrIpAddr = _gHostnameOrIpAddr
  }
  if (_gServerType == UDP) {
    serverAddr := hostnameOrIpAddr + ":" + _gPort
    udpAddr, err := net.ResolveUDPAddr("udp", serverAddr)
    if err == nil {
      _gUdpSocket, err = net.ListenUDP("udp", udpAddr)
      return (true)
    } else {
      return (false)
    }
  } else if (_gServerType == UNIX) {
    _gUnixSourceAddress = _gUnixSocketPath + _gServerName
    os.Remove(_gUnixSourceAddress)
    unixAddr, err := net.ResolveUnixAddr("unixgram", _gUnixSourceAddress)
    if err == nil {
      _gUnixSocket, err = net.ListenUnixgram("unixgram", unixAddr)
      return (true)
    } else {
      return (false)
    }
    return (true)
  } else if (_gServerType == TCP) {
    // Listen for incoming connections
    serverAddr := hostnameOrIpAddr + ":" + _gPort
    tcpAddr, err := net.ResolveTCPAddr("tcp", serverAddr)
    if err == nil {
      _gTcpSocket, err = net.ListenTCP("tcp", tcpAddr)
      return (true)
    } else {
      return (false)
    }
  } else {
    return (false)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func receiveDGRAM() {
  var err error
  var recvSize int
  if (_gServerType == UDP) {
    recvSize, _gRecvAddr, err = _gUdpSocket.ReadFrom(_gPshellRcvMsg)
  } else if (_gServerType == UNIX) {
    recvSize, _gRecvAddr, err = _gUnixSocket.ReadFrom(_gPshellRcvMsg)
  }
  if (err == nil) {
    processCommand(getPayload(_gPshellRcvMsg, recvSize))
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func addTabCompletions() {
  if (((_gServerType == LOCAL) || (_gServerType == TCP)) &&
       (_gRunning  == true)) {
    for _, entry := range(_gCommandList) {
      addTabCompletion(entry.command)
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func addTabCompletion(keyword_ string) {
  for _, keyword := range(_gTabCompletions) {
    if (keyword == keyword_) {
      //duplicate keyword found, return
      return
    }
  }
  if (len(keyword_) > _gMaxTabCompletionKeywordLength) {
    _gMaxTabCompletionKeywordLength = len(keyword_)+5
    _gMaxCompletionsPerLine = 80/_gMaxTabCompletionKeywordLength
  }
  _gTabCompletions = append(_gTabCompletions, keyword_)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func findTabCompletions(keyword_ string) []string {
  var matchList []string
  for _, keyword := range(_gTabCompletions) {
    if (isSubString(keyword_, keyword, len(keyword_))) {
      matchList = append(matchList, keyword)
    }
  }
  return (matchList)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func findLongestMatch(matchList_ []string, command_ string) string {
  command := command_
  charPos := len(command_)
  for {
    if (charPos < len(matchList_[0])) {
      char := matchList_[0][charPos]
      for _, keyword := range(matchList_) {
         if ((charPos >= len(keyword)) || (char != keyword[charPos])) {
           return (command)
         }
       }
      command = command + string(char)
      charPos += 1
    } else {
      return (command)
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func showTabCompletions(completionList_ []string, prompt_ string) {
  if (len(completionList_) > 0) {
    printf("\n")
    totPrinted := 0
    numPrinted := 0
    for _, keyword := range(completionList_) {
      printf("%-*s", _gMaxTabCompletionKeywordLength, keyword)
      numPrinted += 1
      totPrinted += 1
      if ((numPrinted == _gMaxCompletionsPerLine) &&
          (totPrinted < len(completionList_))) {
        printf("\n")
        numPrinted = 0
      }
    }
    printf("\n%s", prompt_)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func clearLine(cursorPos_ int, command_ string) {
  printf("%s%s%s",
         strings.Repeat("\b", cursorPos_),
         strings.Repeat(" ", cursorPos_),
         strings.Repeat("\b", len(command_)))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func beginningOfLine(cursorPos_ int, command_ string) int {
  if (cursorPos_ > 0) {
    cursorPos_ = 0
    printf(strings.Repeat("\b", len(command_)))
  }
  return (cursorPos_)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func endOfLine(cursorPos_ int, command_ string) int {
  if (cursorPos_ < len(command_)) {
    printf(command_[cursorPos_:])
    cursorPos_ = len(command_)
  }
  return cursorPos_
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func killLine(cursorPos_ int, command_ string) (int, string) {
  clearLine(cursorPos_, command_)
  command_ = ""
  cursorPos_ = 0
  return cursorPos_, command_
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func showCommand(command_ string) (int, string) {
  printf(command_)
  return len(command_), command_
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getInput(command_ string,
              keystroke_ []byte,
              length_ int,
              cursorPos_ int,
              tabCount_ int,
              prompt_ string) (string,
                               bool,
                               bool,
                               int,
                               int) {
  quit := false
  fullCommand := false
  if (keystroke_[0] == _CR) {
    // user typed CR, indicate the command is entered and return
    printf("\n")
    tabCount_ = 0
    if (len(command_) > 0) {
      fullCommand = true
      if (len(_gCommandHistory) == 0 ||
          _gCommandHistory[len(_gCommandHistory)-1] != command_) {
        _gCommandHistory = append(_gCommandHistory, command_)
        _gCommandHistoryPos = len(_gCommandHistory)
      }
    }
  } else if ((length_ == 1) &&
             (keystroke_[0] >= _SPACE) &&
             (keystroke_[0] < _DEL)) {
    // printable single character, add it to our command,
    command_ = command_[:cursorPos_] +
               string(keystroke_[0]) +
               command_[cursorPos_:]
    printf("%s%s",
           command_[cursorPos_:],
           strings.Repeat("\b", len(command_[cursorPos_:])-1))
    cursorPos_ += 1
    tabCount_ = 0
  } else {
    inEsc := false
    var esc byte = 0
    // non-printable character or escape sequence, process it
    for index := 0; index < length_; index++ {
      //fmt.Printf("index[%d], val: %d\n", index, keystroke_[index])
      char := keystroke_[index]
      if (char != _TAB) {
        // something other than TAB typed, clear out our tabCount
        tabCount_ = 0
      }
      if (inEsc == true) {
        if (esc == '[') {
          if (char == 'A') {
            // up-arrow key
            if (_gCommandHistoryPos > 0) {
              _gCommandHistoryPos -= 1
              clearLine(cursorPos_, command_)
              cursorPos_, command_ = showCommand(_gCommandHistory[_gCommandHistoryPos])
            }
            inEsc = false
            esc = 0
          } else if (char == 'B') {
            // down-arrow key
            if (_gCommandHistoryPos < len(_gCommandHistory)-1) {
              _gCommandHistoryPos += 1
              clearLine(cursorPos_, command_)
              cursorPos_, command_ = showCommand(_gCommandHistory[_gCommandHistoryPos])
            } else {
              // kill whole line
              _gCommandHistoryPos = len(_gCommandHistory)
              cursorPos_, command_ = killLine(cursorPos_, command_)
            }
            inEsc = false
            esc = 0
          } else if (char == 'C') {
            // right-arrow key
            if (cursorPos_ < len(command_)) {
              printf("%s%s",
                     command_[cursorPos_:],
                     strings.Repeat("\b", len(command_[cursorPos_:])-1))
              cursorPos_ += 1
            }
            inEsc = false
            esc = 0
          } else if (char == 'D') {
            // left-arrow key
            if (cursorPos_ > 0) {
              cursorPos_ -= 1
              printf("\b")
            }
            inEsc = false
            esc = 0
          } else if (char == '1') {
            printf("home2")
            cursorPos_ = beginningOfLine(cursorPos_, command_)
          //} else if (char == '3') {
          //  printf("delete\n")
          } else if (char == '~') {
            // delete key, delete under cursor
            if (cursorPos_ < len(command_)) {
              printf("%s%s%s",
                     command_[cursorPos_+1:],
                     " ",
                     strings.Repeat("\b", len(command_[cursorPos_:])))
              command_ = command_[:cursorPos_] + command_[cursorPos_+1:]
            }
            inEsc = false
            esc = 0
          } else if (char == '4') {
            printf("end2\n")
            cursorPos_ = endOfLine(cursorPos_, command_)
          }
        } else if (esc == 'O') {
          if (char == 'H') {
            // home key, go to beginning of line
            cursorPos_ = beginningOfLine(cursorPos_, command_)
          } else if (char == 'F') {
            // end key, go to end of line
            cursorPos_ = endOfLine(cursorPos_, command_)
          }
          inEsc = false
          esc = 0
        } else if ((char == '[') || (char == 'O')) {
          esc = char
        } else {
          inEsc = false
        }
      } else if (char == 11) {
        // kill to eol
        printf("%s%s",
               strings.Repeat(" ", len(command_[cursorPos_:]) ),
               strings.Repeat("\b", len(command_[cursorPos_:])))
        command_ = command_[:cursorPos_]
      } else if (char == 21) {
        // kill whole line
        cursorPos_, command_ = killLine(cursorPos_, command_)
      } else if (char == _ESC) {
        // esc character
        inEsc = true
      } else if ((char == _TAB) &&
                ((len(command_) == 0) ||
                 (len(strings.Split(strings.TrimSpace(command_), " ")) == 1))) {
        // tab character, print out any completions, we only do
        // tabbing on the first keyword
        tabCount_ += 1
        if (tabCount_ == 1) {
          // this tabbing method is a little different than the standard
          // readline or bash shell tabbing, we always trigger on a single
          // tab and always show all the possible completions for any
          // multiple matches, this is referred to as 'fast' tabbing
          if (len(command_) == 0) {
            // nothing typed, just TAB, show all registered TAB completions
            showTabCompletions(_gTabCompletions, prompt_)
          } else {
            // partial word typed, show all possible completions
            matchList := findTabCompletions(command_)
            if (len(matchList) == 1) {
              // only one possible completion, show it
              clearLine(cursorPos_, command_)
              cursorPos_, command_ = showCommand(matchList[0] + " ")
            } else if (len(matchList) > 1) {
              // multiple possible matches, fill out longest match and
              // then show all other possibilities
              clearLine(cursorPos_, command_)
              cursorPos_, command_ = showCommand(findLongestMatch(matchList, command_))
              showTabCompletions(matchList, prompt_+command_)
            }
          }
        }
      } else if (char == _DEL) {
        // backspace delete
        if ((len(command_) > 0) && (cursorPos_ > 0)) {
          printf("%s%s%s%s",
                 "\b",
                 command_[cursorPos_:],
                 " ",
                 strings.Repeat("\b", len(command_[cursorPos_:])+1))
          command_ = command_[:cursorPos_-1] + command_[cursorPos_:]
          cursorPos_ -= 1
        }
      } else if (char == 1) {
        // home, go to beginning of line
        cursorPos_ = beginningOfLine(cursorPos_, command_)
      } else if ((char == 3) && (_gServerType == LOCAL)) {
        // ctrl-c, raise signal SIGINT to our own process
        //os.kill(os.getpid(), signal.SIGINT)
      } else if (char == 5) {
        // end, go to end of line
        cursorPos_ = endOfLine(cursorPos_, command_)
      } else if (char != _TAB) {
        // don't print out tab if multi keyword command
        //_write("\nchar value: %d" % char)
        //_write("\n"+prompt_)
      }
    }    
  }
  return command_, fullCommand, quit, cursorPos_, tabCount_
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func receiveTCP() {
  var fullCommand bool
  var command string
  var length int
  var cursorPos int
  var tabCount int
  var err error
  _gConnectFd.Write(_gTcpNegotiate)
  _gConnectFd.Read(_gPshellRcvMsg)
  showWelcome()
  _gQuitTcp = false
  command = ""
  fullCommand = false
  cursorPos = 0
  tabCount = 0
  _gCommandHistory = []string{}
  for (_gQuitTcp == false) {
    if (command == "") {
      printf("\r%s", _gTcpPrompt)
    }
    _gConnectFd.SetReadDeadline(time.Now().Add(time.Minute*time.Duration(_gTcpTimeout)))
    length, err = _gConnectFd.Read(_gPshellRcvMsg)
    if (err == nil) {
      command,
      fullCommand,
      _gQuitTcp,
      cursorPos,
      tabCount = getInput(command,
                          _gPshellRcvMsg,
                          length,
                          cursorPos,
                          tabCount,
                          _gTcpPrompt)
      if ((_gQuitTcp == false) && (fullCommand == true)) {
        processCommand(command)
        command = ""
        fullCommand = false
        cursorPos = 0
      }
    } else {
      printf("\nIdle session timeout\n")
      _gQuitTcp = true
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func isValidArgCount() bool {
  return ((len(_gArgs) >= _gFoundCommand.minArgs) &&
          (len(_gArgs) <= _gFoundCommand.maxArgs))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryVersion() {
  printf("%s", _gServerVersion)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryPayloadSize() {
  printf("%d", _gPshellMsgPayloadLength)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryName() {
  printf(_gServerName)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryTitle() {
  printf(_gTitle)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryBanner() {
  printf(_gBanner)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryPrompt() {
  printf(_gPrompt)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryCommands1() {
  for _, command := range _gCommandList {
    printf("%-*s  -  %s\n", _gMaxLength, command.command, command.description)
  }
  printf("\n")
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryCommands2() {
  for _, command := range _gCommandList {
    printf("%s%s", command.command, "/")
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processCommand(command_ string) {
  if (getMsgType(_gPshellRcvMsg) == _QUERY_VERSION) {
    processQueryVersion()
  } else if (getMsgType(_gPshellRcvMsg) == _QUERY_PAYLOAD_SIZE) {
    processQueryPayloadSize()
  } else if (getMsgType(_gPshellRcvMsg) == _QUERY_NAME) {
    processQueryName()
  } else if (getMsgType(_gPshellRcvMsg) == _QUERY_TITLE) {
    processQueryTitle()
  } else if (getMsgType(_gPshellRcvMsg) == _QUERY_BANNER) {
    processQueryBanner()
  } else if (getMsgType(_gPshellRcvMsg) == _QUERY_PROMPT) {
    processQueryPrompt()
  } else if (getMsgType(_gPshellRcvMsg) == _QUERY_COMMANDS1) {
    processQueryCommands1()
  } else if (getMsgType(_gPshellRcvMsg) == _QUERY_COMMANDS2) {
    processQueryCommands2()
  } else {
    _gCommandDispatched = true
    _gArgs = strings.Split(strings.TrimSpace(command_), " ")
    command_ := _gArgs[0]
    if (len(_gArgs) > 1) {
      _gArgs = _gArgs[1:]
    } else {
      _gArgs = []string{}
    }
    numMatches := 0
    if ((command_ == "?") || (command_ == "help")) {
      help(_gArgs)
      _gCommandDispatched = false
      return
    } else {
      for _, entry := range _gCommandList {
        if (isSubString(command_, entry.command, len(command_))) {
          _gFoundCommand = entry
          numMatches += 1
        }
      }
    }
    if (numMatches == 0) {
      printf("PSHELL_ERROR: Command: '%s' not found\n", command_)
    } else if (numMatches > 1) {
      printf("PSHELL_ERROR: Ambiguous command abbreviation: '%s'\n", command_)
    } else {
      if (IsHelp()) {
        if (_gFoundCommand.showUsage == true) {
          ShowUsage()          
        } else {
          _gFoundCommand.function(_gArgs)
        }
      } else if (!isValidArgCount()) {
        ShowUsage()
      } else {
        _gFoundCommand.function(_gArgs)
      }
    }
  }
  _gCommandDispatched = false
  reply(_COMMAND_COMPLETE)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func reply(response_ byte) {
  pshellSendMsg := createMessage(response_, 
                                 getRespNeeded(_gPshellRcvMsg), 
                                 getDataNeeded(_gPshellRcvMsg), 
                                 getSeqNum(_gPshellRcvMsg), 
                                 _gPshellSendPayload)
  if (_gServerType == UDP) {
    _gUdpSocket.WriteTo(pshellSendMsg, _gRecvAddr)
  } else if (_gServerType == UNIX) {
    _gUnixSocket.WriteTo(pshellSendMsg, _gRecvAddr)
  }
  _gPshellSendPayload = ""
}

////////////////////////////////////////////////////////////////////////////////
//
// PshellMsg datagram message processing functions
//
// A PshellMsg is just a byte slice, there is a small 8 byte header along 
// with an ascii byte payload as follows:
// 
//   type PshellMsg struct {
//     msgType byte
//     respNeeded byte
//     dataNeeded byte
//     pad byte
//     seqNum uint32
//     payload string
//   }
//
// I did not have any luck serializing this using 'gob' or binary/encoder to
// send 'over-the-wire' as-is, so I am just representing this as a byte slice
// and packing/extracting the header elements myself based on byte offsets, 
// everything in the message except the 4 byte seqNum are single bytes, so I
// didn't think this was to bad.  There is probably a correct way to do this
// in 'go', but since I'm new to the language, this was the easiest way I got
// it to work.
//
////////////////////////////////////////////////////////////////////////////////

// create offsets into the byte slice for the various items in the msg header
const (
  _MSG_TYPE_OFFSET = 0
  _RESP_NEEDED_OFFSET = 1
  _DATA_NEEDED_OFFSET = 2
  _SEQ_NUM_OFFSET = 4
  _PAYLOAD_OFFSET = 8
)

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getPayload(message_ []byte, recvSize_ int) string {
  return (string(message_[_PAYLOAD_OFFSET:recvSize_]))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getMsgType(message_ []byte) byte {
  return (message_[_MSG_TYPE_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setMsgType(message_ []byte, msgType_ byte) {
  message_[_MSG_TYPE_OFFSET] = msgType_
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getRespNeeded(message_ []byte) byte {
  return (message_[_RESP_NEEDED_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setRespNeeded(message_ []byte, respNeeded_ byte) {
  message_[_RESP_NEEDED_OFFSET] = respNeeded_
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getDataNeeded(message_ []byte) byte {
  return (message_[_DATA_NEEDED_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setDataNeeded(message_ []byte, dataNeeded_ byte) {
  message_[_DATA_NEEDED_OFFSET] = dataNeeded_
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getSeqNum(message_ []byte) uint32 {
  return (binary.BigEndian.Uint32(message_[_SEQ_NUM_OFFSET:]))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setSeqNum(message_ []byte, seqNum_ uint32) {
  binary.BigEndian.PutUint32(message_[_SEQ_NUM_OFFSET:], seqNum_)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func createMessage(msgType_ byte,
                   respNeeded_ byte,
                   dataNeeded_ byte,
                   seqNum_ uint32,
                   command_ string) []byte {
  message := []byte{msgType_, respNeeded_, dataNeeded_, 0, 0, 0, 0, 0}
  setSeqNum(message, seqNum_)
  message = append(message, []byte(command_)...)
  return (message)
}

