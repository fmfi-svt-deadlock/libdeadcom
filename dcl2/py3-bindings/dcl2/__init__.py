import threading
from ._dcl2 import PlainDeadcomL2


class DeadcomL2(PlainDeadcomL2):

    def _mutexInit(self, context):
        context.mutex = threading.Lock()

    def _condvarInit(self, context):
        context.condvar = threading.Condition(lock=self.mutex)

    def _mutexLock(self, context):
        context.mutex.acquire()

    def _mutexUnlock(self, context):
        context.mutex.release()

    def _condvarWait(self, context, milliseconds):
        return not context.condvar.wait(milliseconds/1000)

    def _condvarSignal(self, context):
        context.condvar.notify()

    def _transmitFunction(self, bytes):
        self.pipe.write(bytes)

    def _receiveFunction(self):
        while True:
            b = self.pipe.read()
            if b == None:
                break
            self.processData(b)

    def __init__(self, pipe):
        """Constructor for a DeadCom Layer 2 link.

        This creates a high-level wrapper around PlainDeadcomL2 and takes care of synchronization
        objects (mutex, condvar and functions operating on them), as well as file-like link
        handling (especially reading from it and feeding that data to processData function).

        `pipe` is an object representing a bidirectional communication link (OS pipe, serial port,
        …) that contains the following methods:

          - pipe.read() reads a single byte from the link. If no bytes are available, it blocks.
            If link gets closed, it returns None
          - pipe.write(bytes) writes bytes to the link. Blocks until the operation is complete.
          - pipe.close() closes the link. Further pipe.write() functions will now be no-op and
            pipe.read() will stop blocking and return None.

        When an object is created from this class a "receive thread" will be started. This thread
        will read incoming bytes from the pipe and pass them to the library for processing.
        The `pipe` object must therefore work correctly in multithreaded environment.

        Rest of API of user-facing API of this class is the same as PlainDeadcomL2.
        """
        self.mutex = None
        self.condvar = None
        self.pipe = pipe
        super().__init__(self, self, self._mutexInit, self._mutexLock, self._mutexUnlock,
                         self._condvarInit, self._condvarWait, self._condvarSignal,
                         self._transmitFunction)
        self.receiveThread = threading.Thread(target=self._receiveFunction)
        self.receiveThread.start()

    def stop(self):
        """Stop the link.

        This function calls close() method on the pipe and stops receive thread. After this
        method is called this class instance will become unusable and needs to be reinitialized.
        """
        self.pipe.close()
        self.receiveThread.join()
