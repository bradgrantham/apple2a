# Apple IIa

Custom ROM for the Apple IIe. Looks mostly like a real Apple IIe, but
when you type `RUN`, the code is compiled instead of interpreted.
Runs between 5 and 30 times faster.

Supported features: The classic way to enter programs with
line numbers, 16-bit integer variables, `HOME`, `PRINT`, `IF/THEN`,
`FOR/NEXT`, `GOTO`, low-res graphics (`GR`, `PLOT`, `COLOR=`, `TEXT`), `REM`,
`DIM` (single-dimensional arrays), `POKE`, and integer and boolean arithmetic.

Not supported: Floating point, strings,
high-res graphics, `DATA/READ/RESUME`, `GOSUB/RETURN/POP`,
multi-dimensional arrays, keyboard input, exponentiation (`A^B`), and cassette I/O.

# Dependencies

* [cc65](https://github.com/cc65/cc65)
* [Apple IIe emulator](https://github.com/bradgrantham/apple2e)

# Running

```
TREES=$HOME/path/to/github/trees make run
```

# License

Copyright 2018 Lawrence Kesteloot and Brad Grantham

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

   http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
