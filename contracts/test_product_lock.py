import hashlib
import io
import tempfile
import unittest
import urllib.error
from unittest.mock import patch
from pathlib import Path

try:
    import contracts.product_lock as product_lock
except ModuleNotFoundError:
    import product_lock


class ProductLockTests(unittest.TestCase):
    def test_verify_digests_checks_all_assets(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            asset = root / "msime.db"
            asset.write_bytes(b"database")
            digest = hashlib.sha256(b"database").hexdigest()
            product_lock.verify_digests(root, {"msime.db": digest})
            asset.write_bytes(b"changed")
            with self.assertRaises(ValueError):
                product_lock.verify_digests(root, {"msime.db": digest})

    def test_verify_digests_rejects_path_traversal(self):
        with tempfile.TemporaryDirectory() as temporary:
            with self.assertRaises(ValueError):
                product_lock.verify_digests(Path(temporary), {"../outside": "0" * 64})

    def test_manifest_provenance_is_bound_to_lock(self):
        calls = []

        def verify_product(directory, profile, required_files):
            calls.append((Path(directory), profile, required_files))
            return {"source": {"repository": "example/repo", "commit": "abc", "dirty": False}}

        manifest = product_lock.verify_manifest_provenance(Path("fixture"), "dictionary-manifest.json", verify_product,
                                                           {"msime.db"}, "example/repo", "abc")
        self.assertEqual(manifest["source"]["commit"], "abc")
        self.assertEqual(calls[0][1:], ("desktop", {"msime.db"}))

    def test_published_checksums_ignores_malformed_lines(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "SHA256SUMS.txt"
            path.write_text("a" * 64 + "  msime.db\nnot a checksum\n", encoding="utf-8")
            self.assertEqual(product_lock.published_checksums(path), {"msime.db": "a" * 64})

    def test_published_checksums_rejects_duplicates(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "SHA256SUMS.txt"
            path.write_text("a" * 64 + "  msime.db\n" + "b" * 64 + "  msime.db\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                product_lock.published_checksums(path)

    def test_resolve_tag_commit_rejects_non_dictionary_tag_before_git(self):
        with patch.object(product_lock.subprocess, "check_output") as check_output:
            with self.assertRaises(ValueError):
                product_lock.resolve_tag_commit("https://example.invalid/repo.git", "latest")
            check_output.assert_not_called()

    def download(self, failures, *, waits=None, **kwargs):
        """Drive one download whose first `failures` attempts raise, recording what it slept for."""
        attempts = 0

        class Response(io.BytesIO):
            def __enter__(self):
                return self

            def __exit__(self, *_):
                self.close()

        def open_url(_url, timeout):
            nonlocal attempts
            attempts += 1
            if attempts <= failures:
                raise urllib.error.URLError("temporary")
            return Response(b"new")

        with tempfile.TemporaryDirectory() as temporary:
            target = Path(temporary) / "asset"
            target.write_bytes(b"old")
            with patch.object(product_lock.urllib.request, "urlopen", side_effect=open_url):
                product_lock.download_with_retries("https://example.invalid/asset", target,
                                                   sleep=(waits if waits is None else waits.append),
                                                   **kwargs)
            return attempts, target.read_bytes()

    def test_download_retries_and_replaces_atomically(self):
        waits = []
        attempts, content = self.download(1, waits=waits, attempts=2, timeout=7)
        self.assertEqual((attempts, content), (2, b"new"))

    def test_retries_are_spaced_and_the_first_attempt_is_immediate(self):
        # Back to back the three attempts land inside the same outage: a host that is briefly
        # unreachable answers all of them the same way, and the caller fails having waited only for
        # its own timeouts. The spacing is the point, so it is asserted rather than the count alone.
        waits = []
        attempts, _ = self.download(2, waits=waits)
        self.assertEqual(attempts, 3)
        self.assertEqual(waits, [5.0, 10.0])

    def test_a_first_attempt_that_works_waits_for_nothing(self):
        waits = []
        attempts, _ = self.download(0, waits=waits)
        self.assertEqual((attempts, waits), (1, []))

    def test_disabling_the_backoff_restores_immediate_retries(self):
        waits = []
        attempts, _ = self.download(2, waits=waits, backoff=0)
        self.assertEqual((attempts, waits), (3, []))

    def test_exhausting_the_attempts_still_reports_the_last_failure(self):
        with self.assertRaises(ValueError) as raised:
            self.download(3, waits=[])
        self.assertIsInstance(raised.exception.__cause__, urllib.error.URLError)

    def test_a_negative_backoff_is_rejected_before_any_request(self):
        with patch.object(product_lock.urllib.request, "urlopen") as urlopen:
            with self.assertRaises(ValueError):
                product_lock.download_with_retries("https://example.invalid/asset", Path("unused"),
                                                   backoff=-1)
            urlopen.assert_not_called()


if __name__ == "__main__":
    unittest.main()
