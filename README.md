PDFCube 0.0.5
=============

[![Build Status](https://drone.io/github.com/baol/pdfcube/status.png)](https://drone.io/github.com/baol/pdfcube/latest)

PDFCube renders PDF presentations with special 3D effects. It adds
eye-candy to your PDF presentations, specially Latex, Beamer and
Prosper ones.

Please see the man page for usage information.

You'll need an OpenGL DRI enabled Xorg. GPL ATI 9250 driver is OK,
i've not tested other cards but had feedback from many users on the
net about a wide range of cards.

This is alpha software (and right now it's a quick hack too), but once
you manage to get it working it's fairly stable and usable.

If you want to hack on the code feel free to contact me at
<mirko.maischberger@gmail.com> either via mail or via jabber.

BUGS
----

This program is intended to present landscape PDF files on 4:3
projection screen, it is not suited to view generic PDF files. If you
try to view portrait files you'll see only the top of each page. This
is not so strange since this program is not intended as a generic PDF
viewer but as a specific presentation tool.

There is a little glitch in the animation when going back with cube
animation. I hope is not too annoying.

If you experience other glitches it's probably due to some compositing
software (KDE Kwin, Compiz or others) this is a limitation in Xorg and
the graphic drivers: you can only have one OpenGL accelerated
application at a time. Try disabling the wm effects.

With some older versions of libpoppler there are some rendering
errors, this should be solved by recent poppler releases.

Conditional compilation macros and Dependencies
-----------------------------------------------

You can define NDEBUG to get rid of debug output. You can try
ENABLE_FOG if you like, the default is to have it off.

 * Linux/BSD with Xorg
 * Poppler (>=0.5.4 recommended) (Debian/Ubuntu package:
   libpoppler-dev libpoppler-glib-dev)
 * GtkGlExt (Debian/Ubuntu package: libgtkglext1-dev)
 * Boost C++ Program Option Library

Contact information
-------------------

Feel free to write to <mirko.maischberger@gmail.com>

Word of Mouth
-------------

If you like this project please support by voting on this sites or blogging it:

 * http://freshmeat.net/projects/pdfcube
 * http://www.gnomefiles.org/app.php/PDF_Cube

Happy Hacking
> Mirko Maischberger
