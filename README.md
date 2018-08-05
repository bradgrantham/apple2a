# Apple 2a

Custom ROM for the Apple 2e. Looks mostly like a real Apple 2e, but
when you type `RUN`, the code is compiled instead of interpreted.
Runs between 5 and 30 times faster.

Supported features: The classic way to enter programs with
line numbers, 16-bit integer variables, `HOME`, `PRINT`, `IF/THEN`,
`FOR/NEXT`, `GOTO`, low-res graphics (`GR`, `PLOT`, `COLOR=`, `TEXT`),
`POKE`, and integer and boolean arithmetic.

Not supported: Floating point, strings,
high-res graphics, `DATA/READ/RESUME`, `GOSUB/RETURN/POP`,
`DIM` (arrays), `REM`, keyboard input, exponentiation (`A^B`), and cassette I/O.

# Dependencies

* [cc65](https://github.com/cc65/cc65)
* [Apple 2e emulator](https://github.com/bradgrantham/apple2e)

# Running

```
TREES=$HOME/path/to/github/trees make run
```
