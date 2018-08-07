# Apple IIa

Custom ROM for the Apple IIe. Looks mostly like a real Apple IIe, but
when you type `RUN`, the code is compiled instead of interpreted.
Runs between 5 and 30 times faster.

Supported features: The classic way to enter programs with
line numbers, 16-bit integer variables, `HOME`, `PRINT`, `IF/THEN`,
`FOR/NEXT`, `GOTO`, low-res graphics (`GR`, `PLOT`, `COLOR=`, `TEXT`),
`DIM` (single-dimensional arrays), `POKE`, and integer and boolean arithmetic.

Not supported: Floating point, strings,
high-res graphics, `DATA/READ/RESUME`, `GOSUB/RETURN/POP`,
`REM`, multi-dimensional arrays, keyboard input, exponentiation (`A^B`), and cassette I/O.

# Dependencies

* [cc65](https://github.com/cc65/cc65)
* [Apple IIe emulator](https://github.com/bradgrantham/apple2e)

# Running

```
TREES=$HOME/path/to/github/trees make run
```
