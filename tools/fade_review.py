#!/usr/bin/env python3
"""Generate synthetic Fade review strips and animations from the headless driver.

Run from the repository root after meson compile -C build. Requires Pillow.
All frames use explicit progress in a single headless context.
"""
import sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tests'))
from transition_fixture import make_pdf
from render_client import Frames
from PIL import Image, ImageDraw
root=Path.cwd()/'docs/transitions/fade-review'
root.mkdir(exist_ok=True)
# Each fixture is generated with valid xref offsets after changing its palette.
import transition_fixture as fixture
source=Path('tests/transition_fixture.py').read_text()
frames=Frames('build/tests/render-driver',root)
sheets=[]
for name,old,new in [('white','0.98 0.98 0.98','0.94 0.96 1'),('dark','0.015 0.025 0.06','0.1 0.025 0.015'),('color','0.12 0.32 0.65','0.65 0.23 0.12')]:
    scope={'__name__':'preview'}
    themed = source.replace('0.12 0.32 0.65',old).replace('0.65 0.23 0.12',new)
    if name == 'white':
        themed = themed.replace('1 1 1 RG', '0.15 0.15 0.15 RG').replace('1 1 1 rg', '0.15 0.15 0.15 rg')
    exec(themed,scope)
    pdf=root/f'{name}.pdf'
    scope['make_pdf'](pdf,effect='Fade')
    sheet=Image.new('RGB',(1600,210),'#202020')
    animation=[]
    for i in range(31):
        output=root/f'{name}-frame.png'
        frames.render('--bgcolor','0.04:0.04:0.04','--snapshot',output,'--snapshot-progress',str(i/30),pdf)
        image=Image.open(output).convert('RGB'); image.thumbnail((640,360)); animation.append(image.copy())
    for column,t in enumerate((0,.25,.5,.75,1)):
        output=root/f'{name}-{column}.png'
        frames.render('--bgcolor','0.04:0.04:0.04','--snapshot',output,'--snapshot-progress',str(t),pdf)
        image=Image.open(output).convert('RGB');image.thumbnail((320,180))
        sheet.paste(image,(column*320,30))
        ImageDraw.Draw(sheet).text((column*320+8,8),f'{name} {t:.0%}',fill='white')
        output.unlink()
    sheet.save(root/f'{name}.png'); sheets.append(sheet)
    animation[0].save(root/f'{name}.gif',save_all=True,append_images=animation[1:],duration=40,loop=0)
    (root/f'{name}-frame.png').unlink()
frames.close()
combined=Image.new('RGB',(1600,630))
for i,sheet in enumerate(sheets):combined.paste(sheet,(0,i*210))
combined.save(root/'contact-sheet.png')
