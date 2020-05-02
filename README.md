# LLVM Server

This is a little TCP server written in C++ which provides an RPC API
to (some of) the functionality of the LLVM compiler engine.

## Why?

The idea is that I would write a compiler for my programming language
in a high-level language (e.g. Common Lisp) and send the generated
LLVM assembly over the wire to LLVM server which would then compile
the code and execute it. I wonder if this would make something like
"hot reload" or "image based development" possible in the world of
low-level programming.

## How to build and test it

Install dependencies:

- LLVM
- clang
- Boost::ASIO

Build:

```
make
```

Build and run all unit tests for the server:

```
make test
```

## Usage

After a successful build, you shall find a `llvm-server` binary in the working directory. Start it.

Once it is running, you shall be able to connect to the server at 127.0.0.1:4000.

For each connection, the server forks a subprocess to handle the
session. As a consequence, there can be several clients at once, each
working in their own session.

## Command protocol

The server implements a simple line-based protocol.

Each client request consists of a single line of text, terminated by
ASCII LF, optionally followed by a command-specific payload whose size
is specified by the command.

The success response looks the same for all commands:

```
OK <size>
<size> bytes of additional information
```

Example:

```
OK 5
hello
```

Error response:

```
ERROR <size>
<size> bytes of error message
```

Example:

```
ERROR 4
FAIL
```

Each session maintains a *module stack* onto which the parsed modules
are pushed. In the command reference, the stack effect describes how
the command affects the module stack.

## PARSE

```
PARSE <size>
<size> bytes of LLVM assembly or bitcode
```

Stack effect: ( -- M )

Parse LLVM assembly (or bitcode) of `<size>` bytes and push the
resulting module to the module stack.

## DUMP

```
DUMP
```

Stack effect: ( M -- M )

Save the module at the top of the module stack into LLVM bitcode
format and return the result in the response.

## LINK

```
LINK
```

Stack effect: ( M1 M2 -- M1+M2 )

Link M2 into M1 and replace the top two stack items with the resulting
module.

## COMMIT

```
COMMIT
```

Stack effect: ( M -- )

Transfer the module at the top of the stack to the execution engine
and remove it from the stack.

## CALL

```
CALL <name> <size>
```

Stack effect: ( -- )

1. Look up function `<name>` in all modules previously
   committed to the execution engine and compile it.
2. Allocate a buffer of `<size>` bytes
3. Invoke the function with the following signature:
   `void (*fn)(void *buf)`
   where `buf` points to the allocated buffer
4. Return the contents of buffer in the response
5. Free buffer

## IMPORT

```
IMPORT <path>
```

Stack effect: ( -- )

Import the shared library at `<path>` into the process.

## QUIT

```
QUIT
```

Stack effect: ( -- )

Close the LLVM server session.
