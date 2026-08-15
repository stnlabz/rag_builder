# Contributing to rag_builder

## Purpose

Contributions to `rag_builder` must preserve the project's deterministic, modular, traceable, and human-governed design.

## Development Expectations

Contributions will:

- Keep the core narrowly focused on orchestration.
- Place specialized behavior in clearly defined modules.
- Preserve source provenance.
- Avoid hidden network dependencies.
- Avoid adding external dependencies without documented justification.
- Treat configuration as data rather than hard-coded environment state.
- Preserve human authority over source approval.
- Use explicit failure handling.
- Include tests for new or changed behavior.

## Source Style

The primary implementation language is ISO C.

Code will favor:

- Small functions
- Explicit ownership
- Bounded memory handling
- Clear return values
- Defensive input validation
- Minimal global state
- Predictable execution paths

## Changes

Bug fixes and maintenance changes may be submitted directly through the normal development process.

Creation of a new module must follow the applicable STN-LABZ Module Creation Request process before implementation begins.

## Pull Requests

Pull requests should include:

- A concise description of the change
- The reason for the change
- Test evidence
- Any affected documentation
- Any policy or architecture impact
