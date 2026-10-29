# Versioning, export and copy provenance

Contract bundle 0.1.0 includes docs, schemas, examples, target/legacy headers and
frozen reference documents. Its manifest records SHA256 for every payload file;
manifest itself is hashed in the export receipt, avoiding self-reference.
Version axes are bundle SemVer0.1.0, native API1, board ABI1.0/platform1,
runtime wire2.0, legacy wire1.0 and independently built product version.
Native API/ABI and wire numbers do not establish implementation availability.

Before acceptance, candidate edits regenerate the manifest under review. After
acceptance **the version's payload is immutable**: any byte change requires a
new bundle version. Incompatible changes additionally bump applicable native
API/board ABI major/wire major; optional compatible additions bump their minor
where available. Pre-1.0 bundle version never excuses changing an accepted copy.
The canonical include tree represents the checkout's selected bundle; older
versions are recovered from their pinned immutable Git commit, not rebuilt with
newer canonical headers. Do not combine docs from one revision with headers from
another. Old exports remain independently verifiable without Git/network.

## Producer workflow

From a clean committed checkout containing the selected manifest:

```sh
python3 interfaces/tools/bundle.py check
python3 interfaces/tools/bundle.py export --output /tmp/emusa80535-interfaces-0.1.0.tar.gz
python3 interfaces/tools/bundle.py verify-archive /tmp/emusa80535-interfaces-0.1.0.tar.gz
```

Export requires every manifest payload to match and exact clean tracked/untracked
bundle inputs. SOURCE_COMMIT is the exact checkout commit (or the source archive's
validated SOURCE_COMMIT when .git absent). Generated BUNDLE-RECEIPT identifies
source repository, commit, bundle and manifest SHA256. A sibling `.receipt.json`
records archive SHA256 and the actual absolute export location; it is not inside
the archive and is not an authenticity signature. Export normalizes paths,
permissions, uid/gid and timestamps for byte-identical archives from identical
source+payload. Tool rejects modified/missing/extra payload on verification.

A Git source archive must contain the checked payload and SOURCE_COMMIT; no
sibling DAP checkout or network is needed for build/install/export. A source
commit string is provenance asserted by the producer, not cryptographic proof
of origin. Distribute the archive hash via a trusted channel or signed release.

## Consumer workflow

Verify archive/hash before compilation, extract to
`vendor/emusa80535-interfaces/0.1.0/`, and create a consumer lock:

```sh
python3 interfaces/tools/bundle.py lock /path/to/emusa80535-interfaces-0.1.0.tar.gz   --consumer external-board --copy-path vendor/emusa80535-interfaces/0.1.0   --output emusa80535-interface.lock.json
```

The lock contains exact bundle/API/ABI/wire versions, source repository/commit,
manifest/archive hashes and copy path. Commit it beside the consumer's build
configuration. `verify-copy ROOT --lock LOCK` checks all payload hashes, exact
inventory and receipt/source identity. Consumers compile with
`-Ivendor/emusa80535-interfaces/0.1.0/lib/include`; they must not copy just one
header out of the bundle. An independently compiled board embeds its own
source/artifact identity and required contract/ABI in its build metadata.

[consumers.json](../consumers.json) records expected owners/roles and adoption
status, not a claim that external copies exist. Each project owns its actual
lock. Installed docs retain the manifest and complete bundle layout, so users
can verify an installed doc copy as well as the canonical installed include
files. No external repository was modified/adopted automatically in this Slice.
