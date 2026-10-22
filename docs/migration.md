# C++20 migration

The active implementation is C++20. The original legacy engine is preserved in Git history at commit `3340626ff657229a83e1379bea6ecdd421d08167` (before the migration).

The legacy master, workers, protocol, shell, examples, Dockerfiles, and their outdated documentation have been replaced by the C++ source, CMake build, typed workload CLI, C++ HTTP API, and Compose configuration. The engine runs as native C++ executables.

The old static portfolio contained legacy examples and a browser simulation. Its replacement describes the actual C++ implementation and measured results and explicitly distinguishes the static GitHub Pages overview from the live coordinator-served dashboard.

The measurements dated October 5 are retained with their original date; they are not new October 22 measurements. See the verification record for subsequent migration checks.

## Public demo publishing

The public demo moved to [Netlify](https://sourya-dce.netlify.app/). See [website notes](website.md) for deployment instructions. The former GitHub Pages URL redirects visitors to Netlify.
