# Architecture

## Overview

`rag_builder` is a configurable, modular ISO C application that converts approved source material into structured knowledge suitable for AI retrieval.

The application separates orchestration from specialized processing.

```text
Source
  |
  v
rag_builder Core
  |
  +-- Source Module
  +-- Parser Module
  +-- Classification Module
  +-- Chunking Module
  +-- Metadata Module
  +-- Relationship Module
  +-- Trust Chain Module
  +-- Validation Module
  |
  v
RAG Build
  |
  v
AI Knowledge Repository
```

## Core Responsibility

The core will coordinate the processing pipeline.

The core will not embed source-specific policy semantics that belong in modules.

Core responsibilities include:

- Configuration loading
- Module orchestration
- Pipeline state
- Error handling
- Logging
- Build identity
- Output coordination

## Module Responsibility

Modules perform bounded processing functions through explicit interfaces.

Planned module classes include:

- Source adapters
- Parsers
- Classifiers
- Chunkers
- Metadata processors
- Relationship processors
- Trust Chain validators
- Deduplication processors
- Conflict detectors
- Output writers

## Initial Processing Model

The first supported workflow is:

```text
Local Policy Directory
        |
        v
Markdown Discovery
        |
        v
STN-LABZ Policy Parser
        |
        v
Metadata Extraction
        |
        v
Knowledge Unit Creation
        |
        v
Trust / Integrity Validation
        |
        v
Build Manifest
        |
        v
Digit-Compatible RAG Output
```

## Source Authority

Generated RAG artifacts are derived data.

The approved source document remains authoritative.

## Extensibility

Future source types may include approved intelligence, architecture documents, engineering references, journals, and build or test artifacts.

Support for new source types will be added through modules rather than by expanding the core into source-specific logic.
