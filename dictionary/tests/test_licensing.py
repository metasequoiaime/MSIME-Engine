import os
from pathlib import Path
import subprocess
import sys
import unittest

REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT))
from licensing import ENV_FLAG, FALLBACKS, UNLICENSED_INPUTS, describe_exclusions, include_unlicensed, is_excluded

import build_all


class Defaults(unittest.TestCase):
    def setUp(self):
        self._saved = os.environ.pop(ENV_FLAG, None)

    def tearDown(self):
        if self._saved is None:
            os.environ.pop(ENV_FLAG, None)
        else:
            os.environ[ENV_FLAG] = self._saved

    def test_unlicensed_inputs_are_excluded_unless_asked_for(self):
        self.assertFalse(include_unlicensed())
        for identifier in UNLICENSED_INPUTS:
            self.assertTrue(is_excluded(identifier), identifier)

    def test_the_opt_in_turns_every_exclusion_off(self):
        for value in ('1', 'yes', 'true'):
            os.environ[ENV_FLAG] = value
            self.assertTrue(include_unlicensed(), value)
            self.assertEqual(describe_exclusions(), [])
        # Explicitly-off spellings must not read as an opt-in.
        for value in ('', '0', 'false', 'False'):
            os.environ[ENV_FLAG] = value
            self.assertFalse(include_unlicensed(), value)

    def test_licensed_inputs_are_never_excluded(self):
        for identifier in ('cn/BaseDictIceV1.txt', 'cn/Wubi86.txt', 'en/BaseDictIceEn.txt', 'ECDICT'):
            self.assertFalse(is_excluded(identifier), identifier)

    def test_every_exclusion_is_explained(self):
        for identifier, reason in UNLICENSED_INPUTS.items():
            self.assertTrue(reason.strip(), identifier)
            self.assertIn(identifier, FALLBACKS)
        self.assertEqual(len(describe_exclusions()), len(UNLICENSED_INPUTS))


class StageInputs(unittest.TestCase):
    def setUp(self):
        self._saved = os.environ.pop(ENV_FLAG, None)

    def tearDown(self):
        if self._saved is None:
            os.environ.pop(ENV_FLAG, None)
        else:
            os.environ[ENV_FLAG] = self._saved

    def test_quanpin_swaps_the_merged_source_for_the_licensed_one(self):
        required = build_all.required_paths(build_all.STAGES_BY_NAME['quanpin'])
        self.assertIn('cn/BaseDictIceV1.txt', required)
        self.assertNotIn('cn/BaseDictAllV1Part1.txt', required)
        self.assertNotIn('cn/BaseDictAllV1Part2.txt', required)
        self.assertNotIn('cn/SingleCharWhitelist.txt', required)
        # The licensed inputs the stage also needs are untouched.
        self.assertIn('cn/SingleCharsAllV1.txt', required)

    def test_english_stage_drops_only_the_commercial_extract(self):
        required = build_all.required_paths(build_all.STAGES_BY_NAME['english'])
        self.assertNotIn('en/oaldpe_words.txt', required)
        self.assertIn('en/BaseDictIceEn.txt', required)

    def test_opting_in_restores_the_original_inputs(self):
        os.environ[ENV_FLAG] = '1'
        required = build_all.required_paths(build_all.STAGES_BY_NAME['quanpin'])
        self.assertIn('cn/BaseDictAllV1Part1.txt', required)
        self.assertIn('cn/SingleCharWhitelist.txt', required)
        self.assertNotIn('cn/BaseDictIceV1.txt', required)

    def test_every_required_path_exists_in_the_repository(self):
        # A swapped-in fallback that does not exist would silently skip the stage instead of
        # building a smaller dictionary.
        for stage in build_all.STAGES:
            for path in build_all.required_paths(stage):
                self.assertTrue((REPO_ROOT / path).exists(), f'{stage.name}: {path}')


class Driver(unittest.TestCase):
    def run_build(self, *arguments, include=False):
        env = dict(os.environ)
        env.pop(ENV_FLAG, None)
        return subprocess.run(
            [sys.executable, str(REPO_ROOT / 'build_all.py'), '--only', 'symbols', *arguments],
            cwd=REPO_ROOT, env=env, capture_output=True, text=True, check=False,
        )

    def test_the_default_run_reports_what_it_left_out(self):
        result = self.run_build('--list')
        self.assertEqual(result.returncode, 0, result.stderr)

    def test_the_japanese_lexicon_stage_is_skipped_by_default(self):
        # rime-jp_sela declares no licence, so its stage does not run in a redistributable build.
        self.assertTrue(is_excluded('rime-jp_sela'))
        self.assertEqual(build_all.STAGES_BY_NAME['japanese-lexicon'].needs_reference, 'rime-jp_sela')


if __name__ == '__main__':
    unittest.main()
