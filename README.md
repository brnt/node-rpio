node-rpio
=========

This is a module for NodeJS which wraps Chris Hager's
[RPIO](https://github.com/metachris/RPIO) C library to allow access to the
Raspberry Pi's GPIO pins.

Chris Hager wrote the initial library and Python hooks. I created a NodeJS
interface based on his Python one, but left the underlying C libs unchanged.

This library strives to be API-compatible with the Python library it's derived
from (same function names, etc). That's convenient, but perhaps this library's
greatest benefit over other Raspberry Pi GPIO libraries is that the value of a
GPIO can be set at the same time that the GPIO's mode is set to output. While
this might seem like a little thing, anyone using a RPi to control sprinkler
valves can tell you it's bad to have the GPIO flip on every time it's
initialized.

Building node-rpio
------------------

To build the library, you'll need npm-gyp installed (and obviously NodeJS):

    npm install -g node-gyp

Then, to build the package:

	node-gyp rebuild

Other libraries with the same name
----------------------------------

I apologize for the naming collision with [node-rpio by
jperkin](https://github.com/jperkin/node-rpio) (available in the NPM repos),
but (IMHO) the other library is probably the one that needs renaming as it has
nothing to do with Chris Hager's original RPIO library, nor does it follow its
API.

Copyright
---------

    Copyright (C) 2013 Chris Hager <chris@linuxuser.at> (original C and Python libs)
	Copyright (C) 2013 Brent Thomson (NodeJS wrapper files)
