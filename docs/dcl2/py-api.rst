DeadcomL2 Python bindings
=========================

.. autoclass:: dcl2.DeadcomL2
    :members:


.. class:: dcl2.PlainDeadcomL2(mutex, condvar, mutexInitFunc, mutexLockFunc, mutexUnlockFunc, \
                               condvarInitFunc, condvarWaitFunc, condvarSignalFunc, transmitFunc)

    Constructor of plain Python binding for C library `libdeadcom`.

    .. note:: This class may be useful on its own, but changes are you want to use high-level
              wrapper `dcl2.DeadcomL2` instead.

    This creates an object representing a DeadcomL2 link. DeadcomL2 link provides facilities for
    sending reliable datagrams over unreliable byte-oriented link. Initialy, the logical link is
    disconnected, and may be connected by calling `connect`. If connected, it may be disconnected
    using `disconnect`. Messages may be exchanged over open logical link using functions
    `sendMessage` and `getMessage`.

    Original `dcl2` C library was intended to be platform-agnostic, and the user must provide
    implementation of synchronization functions for multithreaded environment, as well as
    implement a transmit callback. The same thing is true for this Plain wrapper (again, you can
    use higher-level `dcl2.DeadcomL2` which takes care of this for you, and implements required
    methods using `threading` module). The arguments are:

      - `mutex`: An object which will be passed as argument to `mutexInitFunc`, `mutexLockFunc` and
        `mutexUnlockFunc`
      - `condvar`: An object which will be passed as argument to `condvarInitFunc`,
        `condvarWaitFunc` and `condvarSignalFunc`.
      - `mutexInitFunc(mutex)`: This function should initialize a
        `lock (aka mutex) object <https://en.wikipedia.org/wiki/Lock_(computer_science)>`_
      - `mutexLockFunc(mutex)`: This function should acquire a lock for the thread it is called
        from. If the lock is currently held by another thread, it should block until it is released.
      - `mutexUnlockFunc(mutex)`: This function should release a lock held by the current thread.
      - `condvarInitFunc(condvar)`: This function should initialize a
        `condition variable <https://en.wikipedia.org/wiki/Monitor_(synchronization)#Condition_variables>`_
      - `condvarWaitFunc(condvar, timeout)`: This function should block the calling thread until
        either `condvarSignalFunc` is called on the `condvar` object, or `timeout` (number of
        milliseconds) expires. It should return `True`, if the timeout occured, `False` otherwise.
        It is guaranteed that `mutex` is locked by calling `mutexLockFunc(mutex)` before invocation
        of this function.
      - `condvarSignalFunc(condvar)`: Singal a condtition variable `condvar`.
      - `transmitFunc(bytes)`: This function should transmit `bytes` over a physical link. This
        function must be safe to call from both the main and the receive thread.


    .. method:: connect()

        Try to establish a connection with the other side.

        :return: `True` if other side accepted the connection and L2 link is ready, `False`
                 otherwise.
        :raises RuntimeError: if the user attempts to call this function and another connection
                              attempt is already in progress.
        :raises RuntimeError: if an internal error occured in the underlying C library.


    .. method:: disconnect()

        Disconnect a link with the other station. If the link is already disconnected, this
        function is a no-op.

        :raises RuntimeError: if the user attemts to call this function while a connection attempt
                              is in progress.
        :raises RuntimeError: if an internal error occured in the underlying C library.


    .. method:: sendMessage(message)

        Send a message over an open DeadcomL2 link.

        This function transmits a message over a DeadcomL2 link and blocks until an acknowledgment
        is received from the other side. If no correct acknowledgment is received, it will try
        retransmitting the message several times.

        If an instance of `PlainDeadcomL2` is on the other side, this function will block until
        `getMessage()` is called (since that function is the one that transmits the acknowledgemnt).
        In that case when this function returns it is guaranteed that other side received the
        message, and some upper layer at the other side picked it up for further processing.

        If the station does not respond it is assumed that it went away and link will be reset to
        disconencted state. In that case you have to call `connect` again.

        :param bytes message: Message to transmit
        :raises BrokenPipeError: if the link is not currently connected
        :raises ConnectionError: if the link was connected, but the other station did not
                                 acknowledged the message several times. Link was reset to
                                 disconnected state.
        :raises RuntimeError: if an internal error occured in the underlying C library.


    .. method:: getReceivedMsg()

        Get a received message.

        If there is a received message pending, this function will transmit an acknowledgment
        and return that mesasge to the caller.

        :return: None if no message was pending or `bytes`.
        :raises RuntimeError: if an internal error occured in the underlying C library.


    .. method:: processData(bytes)

        Process received bytes.

        Call this function when some bytes were received over the physical link. This function will
        process these bytes, update internal state of the library and execute appropriate reactions
        to received data.

        :param bytes bytes: Received bytes
        :raises RuntimeError: if an internal error occured in the underlying C library.
