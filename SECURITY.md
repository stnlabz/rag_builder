# Security Policy

## Scope

This document describes security expectations for the `rag_builder` repository.

## Security Principles

`rag_builder` will treat all source material as untrusted until the applicable validation and approval requirements are satisfied.

The application will:

- Validate input before processing.
- Preserve source identity and provenance.
- Fail safely when integrity checks fail.
- Avoid silently accepting malformed or unsupported input.
- Avoid unrestricted public Internet dependencies.
- Protect local configuration, credentials, and sensitive build artifacts.
- Record security-relevant failures in build or validation output.

## Reporting Security Issues

Security issues should be reported privately to STN-LABZ rather than disclosed through a public issue before review.

Do not include credentials, secrets, private keys, personal data, or exploitable operational details in public reports.

## Supported Versions

`rag_builder` is in active development. Security support currently applies to the latest development branch and latest published release, when releases begin.
