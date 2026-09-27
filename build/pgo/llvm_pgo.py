# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Collect LLVM profiles outside the directories cleaned between PGO passes."""

import glob
import os
import subprocess
import tempfile


class LLVMProfile(object):
    def __init__(self, topobjdir, profdata):
        if not profdata or not os.path.isfile(profdata):
            raise RuntimeError("clang-cl PGO requires a configured llvm-profdata")
        self.topobjdir = os.path.abspath(topobjdir)
        self.profdata = profdata
        self.output = os.path.join(self.topobjdir, 'merged.profdata')
        self.directory = None

    def prepare(self, env):
        # Never reuse a previous training run's merged or raw profiles.
        if os.path.exists(self.output):
            os.remove(self.output)
        self.directory = tempfile.mkdtemp(prefix='pgo-profiles-', dir=self.topobjdir)
        # %m distinguishes instrumented DLLs; %p distinguishes child processes.
        env['LLVM_PROFILE_FILE'] = os.path.join(self.directory, '%m-%p.profraw')

    def merge(self):
        profiles = sorted(glob.glob(os.path.join(self.directory, '*.profraw')))
        if not profiles or not any(os.path.getsize(p) for p in profiles):
            raise RuntimeError("PGO training produced no LLVM profile data")
        # An input list avoids Windows command-line length limits. Keep profiles
        # on failure so that corrupt or incompatible data can be diagnosed.
        inputs = os.path.join(self.directory, 'profiles.list')
        with open(inputs, 'w') as stream:
            for profile in profiles:
                stream.write(profile + '\n')
        subprocess.check_call([self.profdata, 'merge', '-o', self.output,
                               '-f', inputs])
        if not os.path.isfile(self.output) or not os.path.getsize(self.output):
            raise RuntimeError("llvm-profdata did not produce a merged profile")
