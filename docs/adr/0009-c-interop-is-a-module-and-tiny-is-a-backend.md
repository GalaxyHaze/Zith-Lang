# C interop is a module; the tiny backend is a backend

C interop (the C binder and the C ABI surface) is its own module behind the
external seam, used by the frontend and the backends. The C object emitter (the
tiny backend) is a backend that consumes VIR, alongside the VM.

The C binder is compiler infrastructure, not a driver, so it lives in the core.
The tiny emitter consumes the same VIR the VM does, so it is a backend, not a
separate compiler. Putting interop in a driver would misplace core logic;
putting it in the frontend would mix ABI handling with type analysis.
