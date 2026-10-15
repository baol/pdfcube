#!/usr/bin/env python3
"""Generate a deterministic WiX source file for a staged directory."""

import argparse
import hashlib
import pathlib
import xml.etree.ElementTree as ET


WIX = "http://schemas.microsoft.com/wix/2006/wi"
ET.register_namespace("", WIX)


def element(parent, name, **attributes):
    return ET.SubElement(parent, f"{{{WIX}}}{name}", attributes)


def identifier(prefix, value):
    digest = hashlib.sha256(value.encode()).hexdigest()[:24]
    return f"{prefix}_{digest}"


parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("stage", type=pathlib.Path)
parser.add_argument("version")
parser.add_argument("output", type=pathlib.Path)
parser.add_argument("icon", type=pathlib.Path)
args = parser.parse_args()

root = ET.Element(f"{{{WIX}}}Wix")
product = element(
    root, "Product", Id="*", Name="PDFCube", Language="1033",
    Version=args.version, Manufacturer="PDFCube",
    UpgradeCode="1B87B90B-63BA-4D3F-A419-55875835CA83",
)
element(product, "Package", InstallerVersion="500", Compressed="yes", InstallScope="perMachine")
element(product, "MajorUpgrade", DowngradeErrorMessage="A newer PDFCube is already installed.")
element(product, "MediaTemplate", EmbedCab="yes")
element(product, "Icon", Id="PDFCubeIcon", SourceFile=str(args.icon.resolve()))
element(product, "Property", Id="ARPPRODUCTICON", Value="PDFCubeIcon")

target = element(product, "Directory", Id="TARGETDIR", Name="SourceDir")
program_files = element(target, "Directory", Id="ProgramFiles64Folder")
install = element(program_files, "Directory", Id="INSTALLDIR", Name="PDFCube")

components = []


def add_directory(parent, directory, relative):
    for child in sorted(directory.iterdir(), key=lambda item: item.name.lower()):
        child_relative = relative / child.name
        if child.is_dir():
            node = element(parent, "Directory", Id=identifier("dir", child_relative.as_posix()),
                           Name=child.name)
            add_directory(node, child, child_relative)
        else:
            component_id = identifier("cmp", child_relative.as_posix())
            component = element(parent, "Component", Id=component_id, Guid="*", Win64="yes")
            element(component, "File", Id=identifier("file", child_relative.as_posix()),
                    Source=str(child.resolve()), KeyPath="yes")
            components.append(component_id)


add_directory(install, args.stage, pathlib.Path())
menu = element(target, "Directory", Id="ProgramMenuFolder")
menu_dir = element(menu, "Directory", Id="PDFCubeProgramMenuDir", Name="PDFCube")
shortcut_component = element(menu_dir, "Component", Id="PDFCubeShortcuts", Guid="*", Win64="yes")
element(shortcut_component, "Shortcut", Id="PDFCubeStartMenuShortcut", Name="PDFCube",
        Target="[INSTALLDIR]pdfcube.exe", WorkingDirectory="INSTALLDIR", Icon="PDFCubeIcon")
element(shortcut_component, "RemoveFolder", Id="RemovePDFCubeProgramMenuDir", On="uninstall")
element(shortcut_component, "RegistryValue", Root="HKLM", Key="Software\\PDFCube",
        Name="Installed", Type="integer", Value="1", KeyPath="yes")
components.append("PDFCubeShortcuts")

feature = element(product, "Feature", Id="Complete", Title="PDFCube", Level="1")
for component_id in components:
    element(feature, "ComponentRef", Id=component_id)

ET.indent(root)
args.output.parent.mkdir(parents=True, exist_ok=True)
ET.ElementTree(root).write(args.output, encoding="utf-8", xml_declaration=True)
