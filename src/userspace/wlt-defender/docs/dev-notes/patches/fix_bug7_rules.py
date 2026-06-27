import sys
f = sys.argv[1]  # rules/ebpf.rules
s = open(f).read()

# BUG 7 fix part 3: fix the IOA rules to use pcomm (parent comm) not path.
old = """# Phase 2.5: Process ancestry IOA rules (Indicators of Attack)
# These use 'pcomm' (parent comm) populated by the ancestry BPF map.
# Note: in v1, parent comm is stored in ev.detail for execve events.

rule SHELL_SPAWNED_BY_WEB_SERVER HIGH QUARANTINE_PROC
  desc Web server (nginx/apache/php-fpm) spawned a shell - likely web shell
  condition comm in (sh,bash,zsh,dash) and path contains nginx

rule SHELL_SPAWNED_BY_PYTHON HIGH FREEZE
  desc Python interpreter spawned a shell - scripted attack indicator
  condition comm in (sh,bash,zsh,dash) and path contains python

rule UNEXPECTED_BIN_EXEC MEDIUM LOG
  desc Execution from /tmp or /var/tmp - common malware drop location
  condition path startswith /tmp/"""
new = """# Phase 2.5: Process ancestry IOA rules (Indicators of Attack)
# BUG 7 fix: these use 'pcomm' (parent comm) populated from the ancestry BPF
# map in ringbuf_cb. Old rules used 'path contains nginx' which never matched
# a shell execve (path is /bin/bash, not the parent).

rule SHELL_SPAWNED_BY_WEB_SERVER HIGH QUARANTINE_PROC
  desc Web server (nginx/apache/php-fpm) spawned a shell - likely web shell
  condition comm in (sh,bash,zsh,dash) and pcomm in (nginx,apache2,httpd,php-fpm)

rule SHELL_SPAWNED_BY_PYTHON HIGH FREEZE
  desc Python interpreter spawned a shell - scripted attack indicator
  condition comm in (sh,bash,zsh,dash) and pcomm in (python,python3,python2)

rule UNEXPECTED_BIN_EXEC MEDIUM LOG
  desc Execution from /tmp or /var/tmp - common malware drop location
  condition path startswith /tmp/"""
assert old in s, "IOA rules block not found"
s = s.replace(old, new)
open(f,'w').write(s)
print("BUG 7 part 3: IOA rules use pcomm (was path, which never matched)")
