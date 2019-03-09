# pshell
**A Lightweight, Process-Specific, Embedded Command Line Shell for C/C++/Python/Go Applications**

This package contains all the necessary code, documentation and examples for
building C/C++/Python/Go applications that incorporate a Process Specific Embedded 
Command Line Shell (PSHELL).  The PSHELL library provides a simple, lightweight,
framework and API to embed functions within a C/C++/Python/Go application that can 
be invoked either via a separate client program or directly from the within
application itself.

There is also a control API provided by where any external program can invoke another
program's registered pshell functions (only supported for UDP or UNIX pshell servers).
This will provide direct programmatic control of a remote process' pshell functions 
without having to fork the calling process to call the 'pshell' command line client 
program via the 'system' call.  This provides functionality similar to the familiar 
Remote Procedure Call (RPC) mechanism.

The control API can function as a simple control plane IPC mechanism for inter-process
communication and control.  If all inter-process control is implemented as a collection
of pshell commands, a user can get a programmatic IPC mechanism along with manual CLI/Shell
access via the same code base.  There is no need to write separate code for CLI/Shell
processing and control/message event processing.  All inter-process control can then be
driven and tested manually via one of the interactive client programs (pshell or pshellAggregator).

The control API supports both unicast and 'multicast' (not true subscriber based multicast
like IGMP, it's more like sender based aggregated unicast)  messaging paradigms.  It also 
supports messaging to broadcast pshell servers (i.e. UDP server running at a subnet 
broadcast address, e.g. x.y.z.255).

The Python, 'C', and 'go' versions are consistent with each other at the API level (i.e.
similar functional API, usage, process interaction etc) and fully interoperable with each
other at the protocol level and can be mixed and matched in any combination.  

The prototype for the callback functions follow the paradigms of the 'main' for each
language.  A pshell callback function can be thought of as a collection of mini 'mains'
within the given process that is invoked via its registered keyword.  The arguments are
passed into each function just like they would be passed into the 'main' from the host's
command line shell (i.e. bash) for each language as shown below.  See the included demo
programs for language specific examples.

'C' callback:

`void myFunc(int argc, char *argv[])`

Python callback:

`def myFunc(argv)`

'go' callback:

`func myFunc(argv []string)`

Command line pshell functions can also display information back to the interactive clients 
via a mechanism similar to the familiar 'printf' as follows:

'C' printf:

`void pshell_printf(const char *format, ...)`

Python printf:

`def printf(string)`

'go' printf:

`func Printf(format string, message ...interface{})`

These functions can be invoked via several methods depending on how the internal PSHELL 
server is configured.  The following shows the various PSHELL server types along with their 
associated invokation method:

* TCP Server   : Uses standard telnet interactive client to invoke functions
* UDP Server   : Uses included pshell/pshellAggregator interactive client or control API to invoke functions
* UNIX Server  : Uses included pshell/pshellAggregator interactive client or control API to invoke functions
* LOCAL Server : No client program needed, functions invoked directly from within application 
                 itself via local command line interactive prompting

The functions are dispatched via its registered command name (keyword), along with 0 or more
command line arguments, similar to command line shell processing.

This framework also provides the ability to run in non-server, non-interactive mode.  In this
mode, the registered commands can be dispatched via the host's shell command line directly as 
single shot commands via the main registering multi-call binary program.  In this mode, there 
is no interactive user prompting, and control is returned to the calling host's command line 
shell when the command is complete.  This mode also provides the ability to setup softlink 
shortcuts to each internal command and to invoke those commands from the host's command line 
shell directly via the shortcut name  rather than the parent program name, in a manner similar 
to [Busybox](https://busybox.net/about.html) functionality.  Note this is only available in the
'C' implementation.

This package also provides an optional integrated interactive dynamic trace filtering mechanism that 
can be incorporated into any software that uses an existing trace logging system that uses the `__FILE__`, 
`__LINE__`, `__FUNCTION__`, and `level` paradigm.  If this functionality is not desired, it can be
easily compiled out via the build-time config files.

Along with the optional trace filtering mechanism, there is also an optional integrated trace logging
subsystem and API to show the integration of an existing logging system into the dynamic trace filter
API.  The output of this logging system can be controlled via the trace filter pshell CLI mechanism.
This example logging system can also be compiled out via the build-time config files if an existing
logging system is used.  All of the trace logging/filtering features are only available via the C based
library.

In addition to the infrastructure components, several demo programs are also provided to show the usage
of the various APIs for each component.  See the respective 'demo' directories under each language for
specific examples.

Finally, a stub module/library is provided that will honor the complete API of the normal pshell
library but with all the underlying functionality stubbed out.  This will allow all the pshell 
functionality to easily be completly removed from an application without requiring any code 
changes or recompilation, only a re-link (for static linking) or restart (when using a shared 
library/module acessed via a softlink) of the target program is necessary.

See the full README file for a complete description of all the components, installation, building, and usage.

Note, this package was originally hosted at [code.google.com](https://code.google.com) as 
[RDB-Lite](https://code.google.com/p/rdb-lite), it was re-christened as 'pshell' when it was 
migrated to this hosting service.
