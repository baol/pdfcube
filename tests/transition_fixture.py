"""Generate small, valid, labeled PDFs without a PDF library."""
from pathlib import Path


def make_pdf(path, orientation="H", sizes=((640, 360), (640, 360)), pages=(0, 1)):
    objects = [b"<< /Type /Catalog /Pages 2 0 R >>", b""]
    objects.append(b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>")
    kids = []
    for number in pages:
        width, height = sizes[number]
        page_id = len(objects) + 1
        kids.append(f"{page_id} 0 R")
        transition = f"/Dur 4 /Trans << /S /Blinds /Dm /{orientation} /D 1.25 >>"
        objects.append((
            f"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 {width} {height}] "
            f"/Resources << /Font << /F1 3 0 R >> >> /Contents {page_id + 1} 0 R "
            f"{transition if number == 1 else ''} >>").encode())
        color = "0.12 0.32 0.65" if number == 0 else "0.65 0.23 0.12"
        commands = [f"{color} rg 0 0 {width} {height} re f", "1 1 1 RG 0.5 w"]
        for i in range(1, 12):
            x, y = width * i / 12, height * i / 12
            commands += [f"{x} 0 m {x} {height} l S", f"0 {y} m {width} {y} l S"]
        for label, x, y in (
            ("TOP LEFT", 10, height - 20), ("TOP RIGHT", width - 110, height - 20),
            ("BOTTOM LEFT", 10, 12), ("BOTTOM RIGHT", width - 110, 12),
            ("OUTGOING" if number == 0 else "INCOMING", width / 2 - 50, height / 2),
        ):
            commands.append(f"1 1 1 rg BT /F1 14 Tf {x} {y} Td ({label}) Tj ET")
        stream = "\n".join(commands).encode()
        objects.append(f"<< /Length {len(stream)} >>\nstream\n".encode() + stream + b"\nendstream")
    objects[1] = f"<< /Type /Pages /Kids [{' '.join(kids)}] /Count {len(kids)} >>".encode()
    data = bytearray(b"%PDF-1.4\n")
    offsets = [0]
    for i, obj in enumerate(objects, 1):
        offsets.append(len(data))
        data.extend(f"{i} 0 obj\n".encode() + obj + b"\nendobj\n")
    xref = len(data)
    data.extend(f"xref\n0 {len(offsets)}\n0000000000 65535 f \n".encode())
    for offset in offsets[1:]:
        data.extend(f"{offset:010d} 00000 n \n".encode())
    data.extend(f"trailer\n<< /Root 1 0 R /Size {len(offsets)} >>\nstartxref\n{xref}\n%%EOF\n".encode())
    Path(path).write_bytes(data)


if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="Directory for the example PDFs")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    for orientation, name in (("H", "horizontal"), ("V", "vertical")):
        make_pdf(args.output / f"blinds-{name}.pdf", orientation)
