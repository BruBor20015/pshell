package PshellServer

import "encoding/binary"
import "net"
import "fmt"
import "strings"

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

/////////////////////////////////////////////////////////////////////////////////
//
// global "private" data
//
/////////////////////////////////////////////////////////////////////////////////

// the following enum values are returned by the non-extraction
// based sendCommand1 and sendCommand2 functions
//
// the following "COMMAND" enums are returned by the remote pshell server
// and must match their corresponding values in PshellServer.cc
// msgType return codes for control client
const (
  _COMMAND_SUCCESS = 0
  _COMMAND_NOT_FOUND = 1
  _COMMAND_INVALID_ARG_COUNT = 2
)

// msgType codes between client and server
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

var _gPrompt = "PSHELL> "
var _gTitle = "PSHELL: "
var _gBanner = "PSHELL: Process Specific Embedded Command Line Shell"
var _gServerVersion = "1"
var _gPshellMsgPayloadLength = 2048
var _gPshellMsgHeaderLength = 8
var _gServerType = UDP
var _gServerName = "None"
var _gServerMode = BLOCKING
var _gHostnameOrIpAddr = "None"
var _gPort = "0"
var _gRunning = false
var _gCommandDispatched = false
var _gCommandInteractive = true
var _gArgs []string
var _gFoundCommand pshellCmd

var _gCommandList = []pshellCmd{}
var _gPshellRcvMsg = make([]byte, _gPshellMsgPayloadLength)
var _gPshellSendPayload = ""
var _gUdpSocket *net.UDPConn
var _gRecvAddr net.Addr
var _gMaxLength = 0

/////////////////////////////////
//
// Public functions
//
/////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func AddCommand(function pshellFunction, 
                command string, 
                description string, 
                usage string, 
                minArgs int, 
                maxArgs int, 
                showUsage bool) {
  addCommand(function, command, description, usage, minArgs, maxArgs, showUsage)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func StartServer(serverName string,
                 serverType string,
                 serverMode int,
                 hostnameOrIpAddr string, 
                 port string) {
  startServer(serverName, serverType, serverMode, hostnameOrIpAddr, port)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func Printf(format_ string, message_ ...interface{}) {
  printf(format_, message_...)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func IsHelp() bool {
  return (isHelp())
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func ShowUsage() {
  showUsage()
}

/////////////////////////////////
//
// Private functions
//
/////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func addCommand(function pshellFunction, 
                command string, 
                description string, 
                usage string, 
                minArgs int, 
                maxArgs int, 
                showUsage bool) {  
                  
  if (len(command) > _gMaxLength) {
    _gMaxLength = len(command)
  }
    
  _gCommandList = append(_gCommandList, 
                         pshellCmd{command, 
                                   usage,
                                   description, 
                                   function,
                                   minArgs, 
                                   maxArgs,
                                   showUsage})
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func startServer(serverName string,
                 serverType string,
                 serverMode int,
                 hostnameOrIpAddr string, 
                 port string) {
  if (_gRunning == false) {
    _gServerName = serverName
    _gServerType = serverType
    _gServerMode = serverMode
    _gHostnameOrIpAddr = hostnameOrIpAddr
    _gPort = port
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
func printf(format_ string, message_ ...interface{}) {
  if (_gCommandInteractive == true) {
    _gPshellSendPayload += fmt.Sprintf(format_, message_...)
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
func showUsage() {
  if (len(_gFoundCommand.usage) > 0) {
    Printf("Usage: %s %s\n", _gFoundCommand.command, _gFoundCommand.usage)
  } else {
    Printf("Usage: %s\n", _gFoundCommand.command)
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
  fmt.Printf("PSHELL_INFO: UDP Server: %s Started On Host: %s, Port: %s\n", _gServerName, _gHostnameOrIpAddr, _gPort)
  // startup our UDP server
  //addCommand(_batch, "batch", "run commands from a batch file", "<filename>", 1, 1, True, True)
  if (createSocket()) {
    for {
      receiveDGRAM()
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runUNIXServer() {
  fmt.Printf("PSHELL_INFO: UNIX Server: %s Started\n", _gServerName)
  // startup our UDP server
  //addCommand(_batch, "batch", "run commands from a batch file", "<filename>", 1, 1, True, True)
  if (createSocket()) {
    for {
      receiveDGRAM()
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runLocalServer() {
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func runTCPServer() {
  fmt.Printf("PSHELL_INFO: TCP Server: %s Started On Host: %s, Port: %s\n", _gServerName, _gHostnameOrIpAddr, _gPort)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func createSocket() bool {
  serverAddr := _gHostnameOrIpAddr + ":" + _gPort
  udpAddr, err := net.ResolveUDPAddr("udp", serverAddr)
  if err == nil {
    _gUdpSocket, err = net.ListenUDP("udp", udpAddr)
    return (true)
  } else {
    return (false)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func receiveDGRAM() {
  var err error
  var recvSize int
  recvSize, _gRecvAddr, err = _gUdpSocket.ReadFrom(_gPshellRcvMsg)
  if (err == nil) {
    processCommand(getPayload(_gPshellRcvMsg, recvSize))
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
  Printf("%s", _gServerVersion)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryPayloadSize() {
  Printf("%d", _gPshellMsgPayloadLength)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryName() {
  Printf(_gServerName)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryTitle() {
  Printf(_gTitle)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryBanner() {
  Printf(_gBanner)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryPrompt() {
  Printf(_gPrompt)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryCommands1() {
  for _, command := range _gCommandList {
    Printf("%-*s  -  %s\n", _gMaxLength, command.command, command.description)
  }
  Printf("\n")
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryCommands2() {
  for _, command := range _gCommandList {
    Printf("%s%s", command.command, "/")
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processCommand(command string) {
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
    _gArgs = strings.Split(strings.TrimSpace(command), " ")
    command := _gArgs[0]
    if (len(_gArgs) > 1) {
      _gArgs = _gArgs[1:]
    } else {
      _gArgs = []string{}
    }
    numMatches := 0
    if ((command == "?") || (command == "help")) {
      //help(_gArgs)
      _gCommandDispatched = false
      return
    } else {
      for _, entry := range _gCommandList {
        if (command == entry.command) {
          _gFoundCommand = entry
          numMatches += 1
        }
      }
    }
    if (numMatches == 0) {
      fmt.Printf("PSHELL_ERROR: Command: '%s' not found", command)
    } else if (numMatches > 1) {
      fmt.Printf("PSHELL_ERROR: Ambiguous command abbreviation: '%s'", command)
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
  reply()
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func reply() {
  pshellSendMsg := createMessage(_COMMAND_COMPLETE, 
                                 getRespNeeded(_gPshellRcvMsg), 
                                 getDataNeeded(_gPshellRcvMsg), 
                                 getSeqNum(_gPshellRcvMsg), 
                                 _gPshellSendPayload)
  _gUdpSocket.WriteTo(pshellSendMsg, _gRecvAddr)
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
// everything in the message except the 4 byte seqNum is bytes, so I didn't
// think this was to bad.  There is probably a correct way to do this in 'go',
// but since I'm new to the language, this was the easiest way I got it to 
// work.
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
func getPayload(message []byte, recvSize int) string {
  return (string(message[_PAYLOAD_OFFSET:recvSize]))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getMsgType(message []byte) byte {
  return (message[_MSG_TYPE_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setMsgType(message []byte, msgType byte) {
  message[_MSG_TYPE_OFFSET] = msgType
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getRespNeeded(message []byte) byte {
  return (message[_RESP_NEEDED_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setRespNeeded(message []byte, respNeeded byte) {
  message[_RESP_NEEDED_OFFSET] = respNeeded
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getDataNeeded(message []byte) byte {
  return (message[_DATA_NEEDED_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setDataNeeded(message []byte, dataNeeded byte) {
  message[_DATA_NEEDED_OFFSET] = dataNeeded
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getSeqNum(message []byte) uint32 {
  return (binary.BigEndian.Uint32(message[_SEQ_NUM_OFFSET:]))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setSeqNum(message []byte, seqNum uint32) {
  binary.BigEndian.PutUint32(message[_SEQ_NUM_OFFSET:], seqNum)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func createMessage(msgType byte, respNeeded byte, dataNeeded byte, seqNum uint32, command string) []byte {
  message := []byte{msgType, respNeeded, dataNeeded, 0, 0, 0, 0, 0}
  setSeqNum(message, seqNum)
  message = append(message, []byte(command)...)
  return (message)
}

