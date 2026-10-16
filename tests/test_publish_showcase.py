import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import json

spec = importlib.util.spec_from_file_location('publish_showcase', sys.argv.pop(1))
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
URL = 'https://github.com/user-attachments/assets/1234-abcd'
SHA = 'a' * 40
TEXT = '# PDFCube\n\n' + module.START + '\nPending.\n' + module.END + '\nRest.\n'


class PublicationTests(unittest.TestCase):
    def test_replace_and_idempotence(self):
        result = module.updated_readme(TEXT, URL, SHA)
        self.assertTrue(result.startswith('# PDFCube\n\n'))
        self.assertTrue(result.endswith('\nRest.\n'))
        self.assertIn('\n\n' + URL + '\n\n', result)
        self.assertEqual(module.updated_readme(result, URL, SHA), result)

    def test_reject_malformed_input(self):
        for text, url, sha in ((TEXT, 'https://example.com/demo.mp4', SHA),
                               (TEXT, URL, 'bad'), ('no markers', URL, SHA),
                               (TEXT + module.START, URL, SHA),
                               (module.END + module.START, URL, SHA)):
            with self.assertRaises(ValueError):
                module.updated_readme(text, url, sha)

    def test_reuses_issue_and_updates_from_returned_attachment(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            video, readme = root / 'demo.mp4', root / 'README.md'
            video.write_bytes(b'example video')
            readme.write_text(TEXT)
            argv = ['publish', str(video), '--repo', 'baol/pdfcube', '--revision', SHA,
                    '--readme', str(readme)]
            calls = []
            def gh(*args):
                calls.append(args)
                if args[:2] == ('issue', 'list'):
                    return json.dumps([{'title': module.TITLE, 'number': 7}])
                if args[:2] == ('issue', 'edit'):
                    self.assertIn('--attach', args)
                    body = Path(args[args.index('--body-file') + 1]).read_text()
                    self.assertIn(f'![]({video.resolve()})', body)
                    return ''
                return json.dumps({'body': URL})
            with patch.object(sys, 'argv', argv), patch.object(module, 'gh', gh):
                module.main()
            self.assertEqual(readme.read_text(), module.updated_readme(TEXT, URL, SHA))
            self.assertEqual([c[1] for c in calls], ['list', 'edit', 'view'])

    def test_missing_upload_url_leaves_readme_unchanged(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            video, readme = root / 'demo.mp4', root / 'README.md'
            video.write_bytes(b'video')
            readme.write_text(TEXT)
            with patch.object(sys, 'argv', ['publish', str(video), '--repo', 'baol/pdfcube',
                                           '--revision', SHA, '--readme', str(readme)]), \
                 patch.object(module, 'gh', side_effect=['[]', 'https://github.com/baol/pdfcube/issues/7',
                                                       '{"body":"missing"}']):
                with self.assertRaises(RuntimeError):
                    module.main()
            self.assertEqual(readme.read_text(), TEXT)


unittest.main()
