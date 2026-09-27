# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from distutils.spawn import find_executable

from llvm_pgo import LLVMProfile


class LLVMProfileTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.mkdtemp(prefix='llvm pgo test ')
        self.profile = LLVMProfile(self.directory, sys.executable)
        self.check_call = subprocess.check_call

    def tearDown(self):
        subprocess.check_call = self.check_call
        shutil.rmtree(self.directory)

    def write(self, path, data):
        with open(path, 'w') as stream:
            stream.write(data)

    def test_missing_tool(self):
        with self.assertRaises(RuntimeError):
            LLVMProfile(self.directory, None)

    def test_prepare_isolates_runs(self):
        self.write(self.profile.output, 'stale merged profile')
        env = {}
        self.profile.prepare(env)
        first = self.profile.directory
        self.write(os.path.join(first, 'stale.profraw'), 'stale raw profile')
        self.profile.prepare(env)
        self.assertNotEqual(first, self.profile.directory)
        self.assertFalse(os.path.exists(self.profile.output))
        self.assertEqual(env['LLVM_PROFILE_FILE'],
                         os.path.join(self.profile.directory, '%m-%p.profraw'))
        with self.assertRaises(RuntimeError):
            self.profile.merge()

    def test_empty_profiles(self):
        self.profile.prepare({})
        self.write(os.path.join(self.profile.directory, 'empty.profraw'), '')
        with self.assertRaises(RuntimeError):
            self.profile.merge()

    def test_merge_uses_input_list(self):
        self.profile.prepare({})
        raw = os.path.join(self.profile.directory, 'module-process.profraw')
        self.write(raw, 'raw profile')
        def merge(args):
            self.assertEqual(args[:4], [sys.executable, 'merge', '-o',
                                       self.profile.output])
            self.assertEqual(args[4], '-f')
            with open(args[5]) as stream:
                self.assertEqual(stream.read().splitlines(), [raw])
            self.write(self.profile.output, 'merged profile')
        subprocess.check_call = merge
        self.profile.merge()

    def test_merge_failure_propagates(self):
        self.profile.prepare({})
        self.write(os.path.join(self.profile.directory, 'bad.profraw'), 'bad')
        def fail(args):
            raise subprocess.CalledProcessError(1, args)
        subprocess.check_call = fail
        with self.assertRaises(subprocess.CalledProcessError):
            self.profile.merge()

    def test_merge_requires_output(self):
        self.profile.prepare({})
        self.write(os.path.join(self.profile.directory, 'data.profraw'), 'raw')
        subprocess.check_call = lambda args: None
        with self.assertRaises(RuntimeError):
            self.profile.merge()


class PGOConfigurationTest(unittest.TestCase):
    def setUp(self):
        self.make = (os.environ.get('MAKE') or find_executable('mozmake') or
                     find_executable('make'))
        if not self.make:
            self.skipTest('GNU make is required')
        self.directory = tempfile.mkdtemp(prefix='pgo configuration ')
        config = os.path.join(os.path.dirname(__file__), '..', '..',
                              'config', 'config.mk')
        with open(config) as stream:
            source = stream.read()
        start = source.index('# Reject stale Windows PGO configuration')
        end = source.index('# Enable profile-based feedback', start)
        self.makefile = os.path.join(self.directory, 'Makefile')
        with open(self.makefile, 'w') as stream:
            stream.write(source[start:end] + '\nall:;\n')

    def tearDown(self):
        if hasattr(self, 'directory'):
            shutil.rmtree(self.directory)

    def run_make(self, *variables):
        process = subprocess.Popen([self.make, '-f', self.makefile] +
                                   list(variables), stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT)
        output = process.communicate()[0].decode('utf-8')
        return process.returncode, output

    def test_missing_profdata_fails_before_build(self):
        code, output = self.run_make('MOZ_PGO=1', 'CLANG_CL=1', 'LLVM_PROFDATA=')
        self.assertNotEqual(code, 0)
        self.assertIn('requires LLVM_PROFDATA', output)

    def test_msvc_flags_rejected_for_clang(self):
        code, output = self.run_make('MOZ_PGO=1', 'CLANG_CL=1',
                                    'LLVM_PROFDATA=llvm-profdata.exe',
                                    'PROFILE_GEN_CFLAGS=-GL')
        self.assertNotEqual(code, 0)
        self.assertIn('requires LLVM instrumentation flags', output)

    def test_llvm_configuration_accepted(self):
        code, output = self.run_make('MOZ_PGO=1', 'CLANG_CL=1',
                                    'LLVM_PROFDATA=llvm-profdata.exe',
                                    'PROFILE_GEN_CFLAGS=-clang:-fprofile-instr-generate')
        self.assertEqual(code, 0, output)

    def test_non_pgo_and_msvc_unaffected(self):
        for variables in [('MOZ_PGO=', 'CLANG_CL=1'),
                          ('MOZ_PGO=1', 'CLANG_CL=')]:
            code, output = self.run_make(*variables)
            self.assertEqual(code, 0, output)


if __name__ == '__main__':
    unittest.main()
