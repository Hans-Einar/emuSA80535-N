"""Evidence for both-parent preservation and bounded reconciliation scope."""
import argparse
import collections
import hashlib
import json
from pathlib import Path
import subprocess
import yaml

p = argparse.ArgumentParser()
p.add_argument('--repo', required=True)
p.add_argument('--head', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
repo = Path(a.repo)
master = 'b43fe36b0965b6ac8628677bb6fcc16513d1f567'
prior = '1e588d28fb168a7c5a42c4c7dc4b51f84d29d1ed'
results = {'head': a.head, 'master': master, 'prior_pr_head': prior, 'checks': []}

def git(*args):
    return subprocess.check_output(['git', '-C', str(repo)] + list(args))

def check(name, okay, detail=None):
    results['checks'].append({'name': name, 'passed': bool(okay), 'detail': detail})
    print(('PASS ' if okay else 'FAIL ') + name)

class UniqueLoader(yaml.SafeLoader):
    pass

def mapping(loader, node, deep=False):
    result = {}
    for k, v in node.value:
        key = loader.construct_object(k, deep=deep)
        if key in result:
            raise ValueError('Duplicate YAML key: ' + str(key))
        result[key] = loader.construct_object(v, deep=deep)
    return result

UniqueLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, mapping)
for parent in [master, prior]:
    code = subprocess.run(['git', '-C', str(repo), 'merge-base', '--is-ancestor', parent, a.head]).returncode
    check('ancestor-' + parent, code == 0)
    for label, args in [('parent-diff', [parent, a.head]), ('mergebase-diff', [parent + '...' + a.head])]:
        r = subprocess.run(['git', '-C', str(repo), 'diff', '--check'] + args, capture_output=True)
        check('diff-check-' + parent + '-' + label, r.returncode == 0, r.stdout.decode())

focused = ['emu_debug_' + module + ext for module in ['event', 'trace', 'runtime'] for ext in ['.c', '.h']]
focused += ['tests/test_debug_' + module + '.c' for module in ['event', 'trace', 'runtime']]
focused += ['SDP/05--Design/EMU-DEBUG-DES-008.md']
for path in focused:
    check('accepted-blob-' + path, git('show', prior + ':' + path) == git('show', a.head + ':' + path))

master_paths = git('ls-tree', '-r', '--name-only', master).decode().splitlines()
master_product = [x for x in master_paths if x.endswith(('.c', '.h', '.py', '.mjs'))]
for path in master_product:
    check('master-product-' + path, git('show', master + ':' + path) == git('show', a.head + ':' + path))

changed = git('diff', '--name-only', master, a.head).decode().splitlines()
allowed = set(focused + ['Makefile', 'tests/Makefile', 'README.md'])
out_of_scope = [x for x in changed if not x.startswith(('SDP/', 'doc/')) and x not in allowed]
check('forbidden-scope-file-allowlist', not out_of_scope, out_of_scope)
results['changed_vs_master'] = changed
results['changed_vs_prior'] = git('diff', '--name-status', prior, a.head).decode().splitlines()
results['remerge_diff'] = git('show', '--remerge-diff', '--stat', a.head).decode()

for path in ['SDP/Traceability/CurrentIndex.yaml', 'SDP/Traceability/Relations.yaml']:
    doc = yaml.load(git('show', a.head + ':' + path), Loader=UniqueLoader)
    check('unique-yaml-' + path, isinstance(doc, dict))
    if 'items' in doc:
        absent = [(key, value) for key, value in doc['items'].items()
                  if value.get('source') and not (repo / value['source']).is_file()]
        planned = [(key, value['source']) for key, value in absent
                   if value.get('status') == 'planned' and value.get('state') == 'target']
        missing = [(key, value['source']) for key, value in absent
                   if (key, value['source']) not in planned]
        results['planned_source_placeholders'] = planned
        check('registry-source-files', not missing, missing)

ledger_path = 'SDP/Traceability/Ledger.ndjson'
lines = [line for line in git('show', a.head + ':' + ledger_path).splitlines() if line.strip()]
records = [json.loads(line) for line in lines]
check('ndjson-parse', len(records) == len(lines), len(records))
present = collections.Counter(lines)
for parent in [master, prior]:
    previous = collections.Counter(line for line in git('show', parent + ':' + ledger_path).splitlines() if line.strip())
    missing = previous - present
    check('historical-ledger-preserved-' + parent, not missing, len(missing))
identities = [hashlib.sha256(line).hexdigest() for line in lines]
check('ledger-content-identities-unique', len(identities) == len(set(identities)))
counts = collections.Counter(r.get('event_id') for r in records)
results['historical_event_id_collisions'] = {k: v for k, v in counts.items() if v > 1}
Path(a.output).write_text(json.dumps(results, indent=2, default=str))
raise SystemExit(any(not x['passed'] for x in results['checks']))
