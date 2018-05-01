A word about integration tests
==============================

DeadCom Layer 2 library is designed to be used in multithreaded systems. These tests verify
behavior of this library against itself, and therefore they need to be multithreaded. This however
presents a problem. Unity test framework is not designed to be used from multiple threads.

When a test assertion fails in unity, it will note this failure in its internal structures and will
rewind stack to test runner function. If this happens in a different thread then stack of that
thread is rewound to somewhere it shouldn't be and all hell breaks loose. Moreover, we need to
clean-up after each test,

Therefore we can't call Unity asserts in threads. We need to somehow propagate these asserts to the
main thread. Therefore threads under test have to follow the following protocol:

  - Main thread declares how many threads may assert something using `declareAssertingThreads`
  - Threads may assert something using macro `THREADED_ASSERT(condition)`. This assert is
    unfortunately extremely limited (and will record only line on which that assert failed).
  - Threads must be cancellable (and therefore must contain cancel points in correct sections).
  - When the thread is about to exit normally, it must call `THREAD_EXIT_OK()` macro.
  - Main thread must call `waitForThreadsAndAssert`. This function will wait until either:
    - all threads have called `THREAD_EXIT_OK` (execution of main thread then continues as normal)
    - at least one thread has called `THREADED_ASSERT`, condition of which evaluated to false
      (main thread will fail Unity test)
    - timeout has expired (main thread will fail Unity test)
