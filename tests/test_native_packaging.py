#!/usr/bin/env python3
import pathlib
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET


generator, release_workflow = map(pathlib.Path, sys.argv[1:])
with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    stage = root / "stage"
    (stage / "share" / "data").mkdir(parents=True)
    (stage / "pdfcube.exe").write_bytes(b"test executable")
    (stage / "share" / "data" / "fixture.txt").write_text("fixture\n")
    icon = root / "pdfcube.ico"
    icon.write_bytes(b"test icon")
    output = root / "pdfcube.wxs"
    subprocess.run(
        [sys.executable, generator, stage, "1.2.3", output, icon], check=True
    )
    namespace = {"w": "http://schemas.microsoft.com/wix/2006/wi"}
    document = ET.parse(output)
    product = document.find(".//w:Product", namespace)
    assert product is not None and product.attrib["Version"] == "1.2.3"
    assert len(document.findall(".//w:File", namespace)) == 2
    references = document.findall(".//w:ComponentRef", namespace)
    assert len(references) == 3

workflow = release_workflow.read_text()
for artifact in (
    'pdfcube-${VERSION}-x86_64.AppImage',
    'pdfcube-${VERSION}-macos.dmg',
    'pdfcube-${VERSION}-x86_64.msi',
):
    assert artifact in workflow, artifact
assert "needs: [prepare, macos, windows]" in workflow
assert workflow.index("Create release tag") > workflow.index("Verify release assets")
