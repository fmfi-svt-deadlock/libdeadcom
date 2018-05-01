Reader<->Controller Protocol Application Layer specification
============================================================

This specification defines the Application layer of Reader<->Controller protocol. This protocol
works by exchanging Controller-Reader Protocol Messages (CRPMs) over reliable datagram link
(such as the one provided by "Deadcom Layer 2 Protocol").

Application layer of the Reader <-> Controller protocol has several very simple tasks:

  - Inform the Controller that user ID has been obtained (e.g. by reading a card)
  - Inform the Controller that an error has occurred
  - Command the Reader to start/stop working
  - Inform the Reader of a change in the state of the door


Data encoding in CRPMs
----------------------

Reader and controller communicate by exchanging Controller-Reader Protocol Messages (CRPM). CRPMs
are contain data encoded according to `CBOR (Concise Binary Object Representation) <cbor.io>`_
standard defined in `RFC 7049 <https://tools.ietf.org/html/rfc7049>`_.

Schema of valid CRPM will be described using
`CDDL (Concise Data Definition Language) <https://tools.ietf.org/html/draft-ietf-cbor-cddl-02>`_ [1]_

.. [1] CDDL standard is, unfortunately, not yet mature, and is only an Internet-Draft. For purposes
       of this documentation we will use the newest version available at the time of writing
       (draft-ietf-cbor-cddl-02).


Protocol versioning
-------------------

This protocol uses `Semantic versioning 2.0.0 <https://semver.org/spec/v2.0.0.html>`_.


CRPM schema description
-----------------------

.. literalinclude:: protocol.cddl
