#!/usr/bin/python
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozprofile import FirefoxProfile, Profile, Preferences
from mozprofile.permissions import ServerLocations
from mozrunner import FirefoxRunner, CLI
from mozhttpd import MozHttpd
import json
import socket
import threading
import os
import sys
import shutil
import tempfile
from datetime import datetime
from mozbuild.base import MozbuildObject
from buildconfig import substs
from llvm_pgo import LLVMProfile

PORT = 8888


def wait_for_training(runner, timeout):
  result = runner.wait(timeout=timeout)
  if result is None:
    runner.stop()
    raise RuntimeError("PGO training browser timed out after %s seconds" % timeout)
  if result != 0:
    raise RuntimeError("PGO training browser exited unsuccessfully")


if __name__ == '__main__':
  cli = CLI()
  debug_args, interactive = cli.debugger_arguments()

  build = MozbuildObject.from_environment()
  httpd = MozHttpd(port=PORT,
                   docroot=os.path.join(build.topsrcdir, "build", "pgo"))
  httpd.start(block=False)

  locations = ServerLocations()
  locations.add_host(host='127.0.0.1',
                     port=PORT,
                     options='primary,privileged')

  #TODO: mozfile.TemporaryDirectory
  profilePath = tempfile.mkdtemp()
  try:
    #TODO: refactor this into mozprofile
    prefpath = os.path.join(build.topsrcdir, "testing", "profiles", "prefs_general.js")
    prefs = {}
    prefs.update(Preferences.read_prefs(prefpath))
    interpolation = { "server": "%s:%d" % httpd.httpd.server_address,
                      "OOP": "false"}
    prefs = json.loads(json.dumps(prefs) % interpolation)
    for pref in prefs:
      prefs[pref] = Preferences.cast(prefs[pref])
    profile = FirefoxProfile(profile=profilePath,
                             preferences=prefs,
                             addons=[os.path.join(build.topsrcdir, 'tools', 'quitter', 'quitter@mozilla.org.xpi')],
                             locations=locations)

    env = os.environ.copy()
    env["XPCOM_DEBUG_BREAK"] = "warn"
    # Keep the training workload in the parent process. In particular, the
    # initialization javascript: URL must work without a content subprocess.
    # This environment belongs only to the temporary training browser.
    env["MOZ_FORCE_DISABLE_E10S"] = "1"

    llvm_profile = None
    if substs.get("CLANG_CL") and substs.get("MOZ_PGO"):
      llvm_profile = LLVMProfile(build.topobjdir, substs.get("LLVM_PROFDATA"))
      llvm_profile.prepare(env)

    # For VC12+, make sure we can find the right bitness of pgort1x0.dll
    if not substs.get('HAVE_64BIT_BUILD'):
        for e in ('VS140COMNTOOLS', 'VS120COMNTOOLS'):
            if e not in env:
                continue

            vcdir = os.path.abspath(os.path.join(env[e], '../../VC/bin'))
            if os.path.exists(vcdir):
                env['PATH'] = '%s;%s' % (vcdir, env['PATH'])
                break

    # Run Firefox a first time to initialize its profile
    runner = FirefoxRunner(profile=profile,
                           binary=build.get_binary_path(where="staged-package"),
                           cmdargs=['javascript:Quitter.quit()'],
                           env=env)
    runner.start()
    wait_for_training(runner, 180)

    jarlog = os.getenv("JARLOG_FILE")
    if jarlog:
      env["MOZ_JAR_LOG_FILE"] = os.path.abspath(jarlog)
      print "jarlog: %s" % env["MOZ_JAR_LOG_FILE"]

    cmdargs = ["http://localhost:%d/index.html" % PORT]
    runner = FirefoxRunner(profile=profile,
                           binary=build.get_binary_path(where="staged-package"),
                           cmdargs=cmdargs,
                           env=env)
    runner.start(debug_args=debug_args, interactive=interactive)
    wait_for_training(runner, 600)
    if llvm_profile:
      llvm_profile.merge()
  finally:
    httpd.stop()
    shutil.rmtree(profilePath)
