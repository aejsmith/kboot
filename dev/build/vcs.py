#
# SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
# SPDX-License-Identifier: ISC
#

# Obtain the revision number from the Git repository.
def revision_id():
    from subprocess import Popen, PIPE

    git = Popen(['git', 'rev-parse', '--short', 'HEAD'], stdout = PIPE, stderr = PIPE)
    revision = git.communicate()[0].strip().decode('utf-8')
    if git.returncode != 0:
        return None
    return revision
