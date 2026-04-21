# Third-Party Sources

This directory stores source checkouts for the libraries referenced by the
project blueprint.

- `PhysX/`: NVIDIA PhysX runtime and samples
- `nanobind/`: Python binding layer for the native module
- `scikit-build-core/`: Python build backend reference and local bootstrap aid

Notes:

- `scikit-build-core` is normally consumed as a Python build dependency, but it
  is vendored here because the project brief asked to clone the required
  libraries directly into the repository.
- Generated build outputs inside third-party repositories should stay ignored.
