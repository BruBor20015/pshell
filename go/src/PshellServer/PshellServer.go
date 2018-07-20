package PshellServer

import "encoding/binary"
import "net"
import "fmt"
import "strings"
//import "time"
//import "strings"

// the following enum values are returned by the non-extraction
// based sendCommand1 and sendCommand2 functions
//
// the following "COMMAND" enums are returned by the remote pshell server
// and must match their corresponding values in PshellServer.cc
// msgType return codes for control client
const (
  COMMAND_SUCCESS = 0
  COMMAND_NOT_FOUND = 1
  COMMAND_INVALID_ARG_COUNT = 2
)

// msgType codes between client and server
const (
  QUERY_VERSION = 1
  QUERY_PAYLOAD_SIZE = 2
  QUERY_NAME = 3
  QUERY_COMMANDS1 = 4
  QUERY_COMMANDS2 = 5
  UPDATE_PAYLOAD_SIZE = 6
  USER_COMMAND = 7
  COMMAND_COMPLETE = 8
  QUERY_BANNER = 9
  QUERY_TITLE = 10
  QUERY_PROMPT = 11
  CONTROL_COMMAND = 12
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
// server types
const UDP = "UDP"
const TCP = "TCP"
const UNIX = "UNIX"
const LOCAL = "LOCAL"

// server modes
const BLOCKING = "BLOCKING"
const NON_BLOCKING = "NON_BLOCKING"

var _gPrompt = "PSHELL> "
var _gTitle = "PSHELL: "
var _gBanner = "PSHELL: Process Specific Embedded Command Line Shell"
var _gServerVersion = "1"
var _gPshellMsgPayloadLength = 2048
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
var _gPshellMsg = make([]byte, _gPshellMsgPayloadLength)
var _gUdpSocket *net.UDPConn
var _gRecvAddr net.Addr

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
                 serverMode string,
                 hostnameOrIpAddr string, 
                 port string) {
  startServer(serverName, serverType, serverMode, hostnameOrIpAddr, port)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func Printf(message_ string, a ...interface{}) {
  if (_gCommandInteractive == true) {
    appendPayload(_gPshellMsg, message_)
    fmt.Printf("payload: '%s'\n", getPayload(_gPshellMsg))
  }
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
                 serverMode string,
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
    fmt.Printf("socket %s created\n", serverAddr)
    return (true)
  } else {
    fmt.Printf("socket %s not created\n", serverAddr)
    return (false)
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func receiveDGRAM() {
  var err error
  _, _gRecvAddr, err = _gUdpSocket.ReadFrom(_gPshellMsg)
  if (err == nil) {
    processCommand(string(getPayload(_gPshellMsg)))
  }
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func isValidArgCount() bool {
  return true
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func isHelp() bool {
  return false
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func showUsage() {

}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func processQueryVersion() {
  Printf("%d", _gServerVersion)
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
    Printf("%s  -  %s\n", command.command, command.description)
    //printf("%-*s  -  %s\n" % (_gMaxLength, command["name"], command["description"]))
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
  clearPayload(_gPshellMsg)
  if (getMsgType(_gPshellMsg) == QUERY_VERSION) {
    processQueryVersion()
  } else if (getMsgType(_gPshellMsg) == QUERY_PAYLOAD_SIZE) {
    processQueryPayloadSize()
  } else if (getMsgType(_gPshellMsg) == QUERY_NAME) {
    processQueryName()
  } else if (getMsgType(_gPshellMsg) == QUERY_TITLE) {
    processQueryTitle()
  } else if (getMsgType(_gPshellMsg) == QUERY_BANNER) {
    processQueryBanner()
  } else if (getMsgType(_gPshellMsg) == QUERY_PROMPT) {
    processQueryPrompt()
  } else if (getMsgType(_gPshellMsg) == QUERY_COMMANDS1) {
    processQueryCommands1()
  } else if (getMsgType(_gPshellMsg) == QUERY_COMMANDS2) {
    processQueryCommands2()
  } else {
    _gCommandDispatched = true
    _gArgs = strings.Fields(command)
    command := _gArgs[0]
    _gArgs = _gArgs[1:]
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
      if (isHelp()) {
        if (_gFoundCommand.showUsage == true) {
          showUsage()          
        } else {
          _gFoundCommand.function(_gArgs)
        }
      } else if (!isValidArgCount()) {
        showUsage()
      } else {
        _gFoundCommand.function(_gArgs)
      }
    }
  }
  _gCommandDispatched = false
  setMsgType(_gPshellMsg, COMMAND_COMPLETE)
  reply()
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func reply() {
  _gUdpSocket.WriteTo(_gPshellMsg, _gRecvAddr)
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
  MSG_TYPE_OFFSET = 0
  RESP_NEEDED_OFFSET = 1
  DATA_NEEDED_OFFSET = 2
  SEQ_NUM_OFFSET = 4
  PAYLOAD_OFFSET = 8
)

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getPayload(message []byte) []byte {
  return (message[PAYLOAD_OFFSET:])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setPayload(message []byte, payload string) {
  copy(message[PAYLOAD_OFFSET:], []byte(payload))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func appendPayload(message []byte, payload string) {
  fmt.Printf("appendPayload: '%s'\n", payload)
  newPayload := append(message[PAYLOAD_OFFSET:], []byte(payload)...)
  copy(message, []byte(newPayload))
  //setPayload(message, string(newPayload))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func clearPayload(message []byte) {
  message[PAYLOAD_OFFSET] = 0
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getMsgType(message []byte) byte {
  return (message[MSG_TYPE_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setMsgType(message []byte, msgType byte) {
  message[MSG_TYPE_OFFSET] = msgType
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getRespNeeded(message []byte) byte {
  return (message[RESP_NEEDED_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setRespNeeded(message []byte, respNeeded byte) {
  message[RESP_NEEDED_OFFSET] = respNeeded
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getDataNeeded(message []byte) byte {
  return (message[DATA_NEEDED_OFFSET])
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setDataNeeded(message []byte, dataNeeded byte) {
  message[DATA_NEEDED_OFFSET] = dataNeeded
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func getSeqNum(message []byte) uint32 {
  return (binary.BigEndian.Uint32(message[SEQ_NUM_OFFSET:]))
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func setSeqNum(message []byte, seqNum uint32) {
  binary.BigEndian.PutUint32(message[SEQ_NUM_OFFSET:], seqNum)
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
func createMessage(msgType byte, respNeeded byte, dataNeeded byte, seqNum uint32, command string) []byte {
  message := []byte{msgType, respNeeded, dataNeeded, 0, 0, 0, 0, 0}
  setSeqNum(message, seqNum)
  message = append(message, []byte(command)...)
  return (message)
}

