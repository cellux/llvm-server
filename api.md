# API

The server implements a simple line-based protocol.

Each request consists of a single line of text, terminated by ASCII
LF, optionally followed by a command-specific payload whose size is
specified by the command.

Success response:

```
OK <size>
<size> bytes of additional information
```

Error response:

```
ERROR <size>
<size> bytes of error message
```

## PARSE

```
PARSE <size>
<size> bytes of LLVM assembly or bitcode
```

Stack effect: ( -- M )

Parse LLVM assembly (or bitcode) of <size> bytes and push the
resulting module to the module stack.

## OPT

```
OPT
```

Stack effect: ( M -- M )

Optimize the module at the top of the module stack.

## DUMP

```
DUMP
```

Stack effect: ( M -- M )

Dump the module at the top of the module stack into LLVM bitcode
format and return it in the response.

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

Add the module at the top of the stack to the execution engine and
remove it from the stack.

## CALL

```
CALL <name> <size>
```

Stack effect: ( -- )

1. Look up function `<name>` in the modules previously
   committed to the execution engine and compile it.
2. Allocate a buffer of <size> bytes
3. Invoke the function with the following signature:
   void (*fn)(void *buf)
   where buf points to the allocated buffer
4. Return the contents of buffer in the response
5. Free buffer

## IMPORT

```
IMPORT <path>
```

Stack effect: ( -- )

Import the shared library at <path> into the process.

## QUIT

```
QUIT
```

Stack effect: ( -- )

Close the LLVM server session.
