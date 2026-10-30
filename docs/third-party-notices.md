# Third-party notices

These notices cover third-party components used by the development foundation. Keep the upstream license text with any redistributed copy of the corresponding component, and review the license again if its pinned version changes. The application itself has no license assigned by this document.

## cJSON 1.7.19

- Source archive: [cJSON v1.7.19](https://github.com/DaveGamble/cJSON/archive/v1.7.19.tar.gz)
- License: MIT ([upstream LICENSE](https://github.com/DaveGamble/cJSON/blob/v1.7.19/LICENSE)). Preserve this notice in copies or substantial portions of the software.
- The SHA-256 and independent provenance record are in `tools/versions.json` under `libraries.cjson`.

```text
Copyright (c) 2009-2017 Dave Gamble and cJSON contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

## Unity 2.6.0

- Source archive: [Unity v2.6.0](https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.6.0.tar.gz)
- License: MIT ([upstream LICENSE.txt](https://github.com/ThrowTheSwitch/Unity/blob/v2.6.0/LICENSE.txt)). Preserve this notice in copies or substantial portions of the software.
- The SHA-256 and independent provenance record are in `tools/versions.json` under `libraries.unity`.

```text
The MIT License (MIT)

Copyright (c) <year> 2007-24 Mike Karlesky, Mark VanderVord, Greg Williams
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

## Scintilla 5.6.7

- Binary archive: [official 64-bit SciTE download](https://www.scintilla.org/wscite567.zip). The build foundation uses `wscite/Scintilla.dll`; the full archive is not bundled into the application. The official archive also includes Lexilla and its license notice.
- The project-local tool manifest records the archive SHA-256 and its independent checksum provenance. CMake requires version 5.6.7 and validates that the extracted DLL is a PE x64 binary before copying it beside the smoke-test executable.
- License: [Scintilla, Lexilla, and SciTE License.txt](https://www.scintilla.org/License.txt), a permissive license requiring the copyright and permission notice in copies and supporting documentation. Retain the upstream notice when redistributing the DLL.

```text
License for Lexilla, Scintilla, and SciTE

Copyright 1998-2021 by Neil Hodgson <neilh@scintilla.org>

All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation.

NEIL HODGSON DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS, IN NO EVENT SHALL NEIL HODGSON BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
OR PERFORMANCE OF THIS SOFTWARE.
```

## Development tools

Versions, official download URLs, checksums, and provenance for tools are maintained in `tools/versions.json`. Refer to each upstream license before redistributing any tool binary; tools provisioned only for local development are not automatically application dependencies.
