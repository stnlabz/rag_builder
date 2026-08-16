# Changelog

All notable changes to `rag_builder` will be documented in this file.

The project is currently in active development.

## [Unreleased]

### Added
- Initial STN-LABZ repository baseline.
- ISO C project definition.
- Modular RAG creation architecture.
- Policy-directory ingestion as the initial source model.
- Trust and provenance requirements.
- Core-controlled module governance infrastructure.
- Module discovery from the configured modules directory.
- Module identity and Core API compatibility validation.
- Core-controlled module qualification and lifecycle management.
- Mandatory module qualification with a minimum of 10 tests and negative validation.
- Core-owned persistent module qualification inventory.
- Qualification restoration for unchanged, previously qualified modules without rerunning their qualification suites.
- Explicit separation between module qualification and activation authorization.
- Core-controlled module activation gating.
- Module qualification and lifecycle audit evidence.
- Core logging with configurable log-level filtering.
- `RB-MARKDOWN` module for deterministic Markdown structure processing.
- Approved Markdown source enumeration and bounded file reading.
- Operational Core-to-`RB-MARKDOWN` processing path.
- Structured Markdown block representation with source and content offsets.
- Markdown heading, paragraph, ordered-list-item, and unordered-list-item classification.

### Validated
- `rag_builder` Core qualification: 20/20 PASS.
- Core module-governance qualification: PASS.
- Core logging qualification: PASS.
- `RB-MARKDOWN` qualification: 10/10 PASS.
- `RB-MARKDOWN` negative validation: PASS.
- Persistent module qualification restoration: PASS.
- Core-controlled `RB-MARKDOWN` operational activation: PASS.
- Real approved Markdown document ingestion and parsing: PASS.
- Source and content offset preservation during Markdown parsing: PASS.
- Core regression following module-governance and Markdown integration: PASS.

### Known
- Markdown blockquotes are currently represented as paragraph blocks.
- Windows console output may not render UTF-8 punctuation correctly even when source bytes and offsets are preserved.