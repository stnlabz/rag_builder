# Roadmap

## Current Phase

Architecture and repository baseline.

## Phase 1 — Core Foundation

- Establish ISO C source tree.
- Establish configuration loader.
- Establish module interface.
- Establish logging and build result model.
- Establish local directory source adapter.

## Phase 2 — Policy Ingestion

- Discover Markdown policy documents.
- Parse the STN-LABZ standard policy format.
- Extract document identity and metadata.
- Validate required fields.
- Restrict ingestion to authorized policy state.
- Generate traceable knowledge units.

## Phase 3 — Trust and Build Validation

- Source hashing
- Build manifests
- Trust Chain integration
- Validation reporting
- Failure and quarantine behavior

## Phase 4 — Retrieval Preparation

- Semantic chunking
- Relationship generation
- Deduplication
- Conflict detection
- AI-consumption metadata

## Phase 5 — Additional Source Types

- Approved market intelligence
- Architecture documentation
- Engineering references
- Build and test artifacts
- Other approved STN-LABZ knowledge sources

## Future Direction

The system may support additional consumers beyond Digit while preserving the same governed source-to-RAG build process.
