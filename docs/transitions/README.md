# Transition design

PDFCube interprets PDF transitions as readable motion between complete slides.
Each page keeps its own fitted aspect ratio. Both full-resolution page textures
are prepared before the animation clock starts. At 0% and 100%, the renderer
uses the exact static source and destination page paths.

Geometry and shading are deterministic functions of progress. This makes
interactive playback, explicit-progress captures, and video export reproducible.
The camera stays fixed during page effects; the classic cube and zoom controls
have their own camera motion.

## Effects

| PDF effect | PDFCube interpretation | Parameters |
| --- | --- | --- |
| Box | Classic cube rotation | Cube navigation also selects forward or backward rotation |
| Blinds | Twelve rotating louvers with a cascading start | Horizontal or vertical |
| Split | Two perspective doors on fixed outer hinges | Horizontal or vertical; inward or outward |
| Cover / Uncover | A curved incoming sheet folds over the page, or the outgoing sheet peels away | Four cardinal directions |
| Fly | A light sheet follows a shallow arc, banks, flutters, and lands flat | Arbitrary angle; inward or outward; starting scale |
| Push | Two shallow padded blocks meet, compress, and recover | Four cardinal directions |
| Dissolve | An irregular glass panel fractures into outward-tumbling pieces | Optional thin glass edges with `--experimental` |
| Glitter | A directional front releases fragments and warm sparks | Right, down, or diagonally down-right |
| Wipe | A raised mechanical wiper clears the outgoing slide | Four cardinal directions |
| Fade | Two stationary slides exchange light | Uniform linear-light blend with eased timing |
| Replace | Instant page change | Also used for unrecognized transition types |

Ready-to-open [diagnostic PDFs](../../examples/transitions/README.md) cover the
parameter combinations. The [Beamer showcase](../../examples/showcase/README.md)
presents the effects with vector artwork and labels naming the next transition.

## Playback and PDF metadata

Space and Page Down use the destination page's PDF transition. Arrow keys
select instant navigation; `c` and `a` select forward and backward cube motion.

PDF `/Trans /D` controls animation duration, including fractional seconds.
A nonpositive animation duration selects an instant change. PDF `/Dur` controls
slide dwell time during automatic playback; a positive dwell duration schedules
advancement after the slide settles.

`--program 4:fade` sets a four-second dwell followed by Fade. Programmed effects
use a 750 ms animation and the destination page's applicable PDF parameters.
The final duration/effect pair repeats for the remaining slides. `c` selects
the classic cube and `p` selects an instant change.

| PDF `/Di` | Travel |
| --- | --- |
| 0 | Right |
| 90 | Up |
| 180 | Left |
| 270 | Down |

Cover, Uncover, Push, and Wipe use these four directions and default to right
for missing or other angles. Glitter uses 0, 270, and 315 degrees and defaults
to right. Fly supports arbitrary angles.

## Surface and motion design

### Blinds and Split

Blinds divides the page into twelve thin louvers. The outgoing image appears
on the front and the incoming image on the reverse with correctly oriented
lettering. A cascading start and restrained angular shade give the motion depth.

Split uses two thin doors clipped to the page. `/Dm /V` selects left/right
doors and `/Dm /H` selects top/bottom flaps. `/M /O` opens the outgoing page;
`/M /I` closes the incoming page. The outer hinges stay fixed and the open
position is edge-on. PDF defaults are horizontal and inward.

### Cover and Uncover

A connected sheet uses 64 joined strips with constant arc length to describe
its curve. Cover folds the incoming sheet over the outgoing page; Uncover peels
the outgoing sheet away. The sheet stops edge-on, with restrained angular shading.

| Travel | Cover hinge | Uncover hinge |
| --- | --- | --- |
| Right | Left | Right |
| Up | Bottom | Top |
| Left | Right | Left |
| Down | Top | Bottom |

### Fly

The moving page follows a shallow 3D arc, banks into its path, and makes one
small deterministic flutter. Both motions settle as the page lands flat.
Incoming pages (`/M /I`) start beyond the opposite edge and move in the PDF
`/Di` direction; outgoing pages (`/M /O`) leave in that direction.

`/SS` sets the off-screen scale. Valid scales are greater than zero and at most
one; other values use one. `/B` metadata is retained. Page textures include the
normal opaque white PDF background. The moving sheet can cross the letterbox
area while retaining its aspect ratio.

### Push

The pages are shallow padded blocks whose contact plane travels in the PDF
direction. The outgoing block compresses by about 10% along the travel axis,
bulges by up to 3.5% across it, and makes one damped wobble. The incoming block
yields by up to 2.5%. Both lift slightly and recover fully before landing.
`--top-color` sets their side-face color as well as the classic cube caps.

### Dissolve

An irregular Voronoi partition divides the outgoing page into 64 interlocking
polygons, with a denser pattern near the central impact. Each polygon is a rigid
piece rendered as a triangle fan. The impact immediately separates and tilts the
pieces, with the strongest forward impulse at the center.

Pieces tumble around fixed axes tangent to their flight directions, each with
its own constant angular velocity. Their launch speeds rise continuously to
15% above the initial speed by the end. Perspective is compensated at the pivot
to preserve the speed profile. Continuous outward travel clears every piece
from the viewport by 99% progress. Both faces retain the outgoing page texture.

The default uses thin surfaces. `--experimental` adds a pale rim and a second
textured face, about two pixels thick at a 960-pixel page width. Both variants
share the same trajectory and timing. The renderer uploads all shard vertices
in one dynamic buffer and draws them in one call.

### Glitter

A directional front releases up to 336 textured fragments from the outgoing
page. Pieces rotate, drift, and fade, accompanied by small warm sparks.
Alpha blending bounds brightness. The destination remains behind the pieces.

### Wipe

A rubber lip, bowed pressure spine, raised brackets, and metal pivot form a
three-dimensional wiper. Beveled matte surfaces, directional lighting, and a
soft contact shadow make it readable against the slide. One damped flex settles
during the straight sweep; eased travel carries the blade fully across both edges.

The pixel-aligned clipping edge stays concealed by the blade. The outgoing
composition includes its letterbox, keeping the incoming page visible only in
the revealed region. Scissor regions limit drawing to each page's visible area.
The mesh uses reusable fixed-capacity storage, with lighting evaluated per quad.

### Fade

Fade decodes sRGB into linear light, blends both slide compositions, and encodes
the result back to sRGB. White artwork stays steady and the midpoint preserves
brightness. The weight is smoothstep (`3t² − 2t³`), giving a gentle start and finish.
Each composition includes its fitted page and surrounding background.

![Fade checkpoints on light, dark, and colored slides](fade-review/contact-sheet.png)

Animated examples: [light](fade-review/white.gif), [dark](fade-review/dark.gif),
and [colored](fade-review/color.gif).

## Render and inspect

From a configured checkout with the build/test dependencies, Pillow, Xvfb,
Xauth, and FFmpeg installed:

```sh
meson compile -C build
meson test -C build --print-errorlogs
python3 tools/transition_gallery.py build/src/pdfcube build/transition-gallery
```

Open `build/transition-gallery/index.html` for looping videos, five-frame contact
sheets, individual frames, and input PDFs. The manifest records the source
revision and binary version. `--effects Wipe` selects one effect; `--jobs N`
controls concurrent captures. `--update-docs` writes contact sheets into this
folder. The default 33 samples include exact quarter-progress frames.

The **Transition review gallery** workflow produces the same downloadable bundle
for a selected branch. `python3 tools/fade_review.py` regenerates the light, dark,
and colored Fade examples using explicit progress in one headless context.

The [test contract](../../tests/README.md) describes endpoint, aspect-ratio,
metadata, determinism, timing, and process-isolation coverage. The
[compatibility matrix](../../COMPATIBILITY.md) records platform coverage.
