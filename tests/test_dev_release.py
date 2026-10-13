import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('publisher', Path(__file__).resolve().parents[1] / 'scripts/publish-dev-release.py')
publisher = importlib.util.module_from_spec(spec)
spec.loader.exec_module(publisher)


class DevReleaseTests(unittest.TestCase):
    def runs(self):
        return [dict(id=i, name=name, head_sha='current', head_branch='main', event='push',
                     status='completed', conclusion='success') for i, name in enumerate(publisher.WORKFLOWS, 1)]

    def test_requires_all_three_matching_successful_builds(self):
        runs = self.runs()
        self.assertIsNotNone(publisher.select_runs(runs, 'current'))
        self.assertIsNone(publisher.select_runs(runs[:2], 'current'))
        self.assertIsNone(publisher.select_runs(runs, 'newer'))
        runs[1]['head_sha'] = 'older'
        self.assertIsNone(publisher.select_runs(runs, 'current'))

    def test_pending_failed_and_untrusted_runs_block_publication(self):
        for field, value in [('status', 'in_progress'), ('conclusion', 'failure'),
                             ('head_branch', 'other'), ('event', 'pull_request')]:
            with self.subTest(field=field):
                runs = self.runs()
                runs[0][field] = value
                self.assertIsNone(publisher.select_runs(runs, 'current'))

    def test_newer_failure_does_not_fall_back_to_old_success(self):
        runs = self.runs()
        runs.append(dict(runs[0], id=99, conclusion='failure'))
        self.assertIsNone(publisher.select_runs(runs, 'current'))

    def test_complete_packages_required_and_old_versions_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            names = ['moonlight-eclipse-0.5.7-1.fc44.x86_64.rpm', 'Eclipse-source-0.5.7.zip', 'Eclipse-0.5.7-x86_64.flatpak']
            for name in names[:2]: (root / name).touch()
            with self.assertRaises(ValueError): publisher.collect_assets(root, '0.5.7')
            (root / names[2]).touch()
            self.assertEqual(len(publisher.collect_assets(root, '0.5.7')), 3)
            (root / 'Eclipse-source-0.5.6.zip').touch()
            with self.assertRaises(ValueError): publisher.collect_assets(root, '0.5.7')


if __name__ == '__main__':
    unittest.main()
