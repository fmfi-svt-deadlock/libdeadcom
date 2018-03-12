# DeadCom libraries

This repository hosts Deadlock communication libraries. Those libraries implement protocols that
embedded components of project Deadlock use for communication.

Those libraries are written in a way that makes them easy to integrate with embedded firmware of
Deadlock components. There are, in general, 2 categories of embedded firmwares used in this project:

  - Written in plain C, based on ChibiOS
  - Written in Python, running on some embedded Linux platform

Therefore these libraries are written in plain-C with no dependencies that ChibiOS can't provide,
and contain Python bindings.


## dcl2

DeadCom Layer 2 is a protocol based on (or inspired by) HDLC. It provides a reliable datagram
communication between 2 devices over unreliable byte-oriented point-to-point link (such as RS232
connection). Its implementation is based on open-source project yahdlc with modifications for our
use-case.


## dcrcp

DeadCom Reader<->Controller Protocol is an application protocol used by Reader and Controller.
