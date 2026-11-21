# Third-Party Notices

The installer for WireGuard Credential Provider downloads and installs the following
third-party components. Their licenses are reproduced below as required.

---

## WireGuard for Windows

**Website:** https://www.wireguard.com  
**Repository:** https://github.com/WireGuard/wireguard-windows  
**License:** MIT License

```
Copyright (C) 2019-2022 WireGuard LLC. All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

> "WireGuard" and the WireGuard logo are registered trademarks of Jason A. Donenfeld.
> This project is not affiliated with or endorsed by WireGuard LLC or Jason A. Donenfeld.

---

## YubiKey Manager CLI (ykman)

**Website:** https://www.yubico.com  
**Repository:** https://github.com/Yubico/yubikey-manager  
**License:** Apache License 2.0

```
Copyright 2015-2023 Yubico AB

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
```

The full text of the Apache License 2.0 is available at:
https://www.apache.org/licenses/LICENSE-2.0

---

## Notes

WireGuard Credential Provider does not bundle any of the above components in its source
repository. They are downloaded and installed at setup time by the NSIS installer. The
source code of WireGuard for Windows and YubiKey Manager CLI is available at the
repositories linked above.
