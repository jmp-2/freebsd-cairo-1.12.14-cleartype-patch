A FreeBSD port of cairo 1.12.14 with [cairo-respect-fontconfig.patch](https://github.com/wor/abs-repo/blob/master/cairo/cairo-respect-fontconfig.patch) and [cairo-1.12.16-cleartype.patch](
https://aur.archlinux.org/packages/cairo-cleartype/) patches applied.

Yes, cairo 1.12.16 is in the ports now, but with the cairo-1.12.16-cleartype patch applied, it produces some very weird/bizarre font renderings. So, after fiddling around with it for about 2 hours, i tried the 1.12.14 version - and (after some adjustmens)  it works!

[![screenshot](cairo-1.12.16\(unpatched\)_cairo.1.12.14\(patched\).png)]
DPI: 120x120px, RGB, hintslight + antialising.

Installation
-----
    make reinstall clean && pkg lock cairo


