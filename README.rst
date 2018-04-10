DeadCom libraries
=================

This repository hosts specification of Deadlock communication protocols and libraries that implement
them.

Those libraries are written in a way that makes them easy to integrate with embedded firmware of
Deadlock components. There are, in general, 2 categories of embedded firmwares used in this project:

  - Written in plain C, based on ChibiOS
  - Written in Python, running on some embedded Linux platform

Therefore, if a library implemented here is intended to be used with embedded components, then it is
written in plain-C with (optional) Python bindings. If a library is designed to be used in
multithreaded systems it uses only features that ChibiOS can provide.


Reader Physical Interface
-------------------------

These docs describe the physical interface for interconnecting Readers and Controllers

.. toctree::
    :maxdepth: 2
    :caption: Reader Physical Interface
    :hidden:

    reader-phys/interface


Deadcom Layer 2 (``dcl2``)
--------------------------

DeadCom Layer 2 is a protocol based on (or inspired by) HDLC. It provides a reliable datagram
communication between 2 devices over unreliable byte-oriented point-to-point link (such as RS232
connection). Its implementation is based on open-source project yahdlc with modifications for our
use-case.

.. toctree::
    :maxdepth: 2
    :caption: Deadcom Layer 2
    :hidden:

    dcl2/protocol
    dcl2/c-api
    dcl2/py-api


Deadcom Reader-Controller Protocol (``dcrcp``)
----------------------------------------------

DeadCom Reader<->Controller Protocol is an application protocol used for communication between
Reader and Controller. It uses CBOR to encode data according to a defined schema. C implementation
intended for embedded systems (using open-source project
`cn-cbor <https://github.com/cabo/cn-cbor>`_) is provided. For Python there are many mature
libraries that can convert CBOR to native Python structures. Anything on top of that would be an
unnecessary layer.

.. toctree::
    :maxdepth: 2
    :caption: Deadcom Reader Protocol (dcrcp)
    :hidden:

    dcrcp/protocol
    dcrcp/c-api


Leaky Pipe (``leaky-pipe``)
---------------------------

Thread-safe byte-oriented point-to-point link emulator with configurable deterministic
unreliability. This is basically a thread-safe blocking queue (based on open-source project
`pipe` https://github.com/cgaebel/pipe), limited so that it only transfers bytes and loses or
corrupts data (leaks added).

Primarily intended for testing other protocol libraries in this repo.

.. toctree::
    :maxdepth: 2
    :caption: Leaky Pipe
    :hidden:

    leaky-pipe/api
