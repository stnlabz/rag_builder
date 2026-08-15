# rag_builder

![Language](https://img.shields.io/badge/Language-ISO%20C-blue)
![Status](https://img.shields.io/badge/Status-Active%20Development-orange)
![Architecture](https://img.shields.io/badge/Architecture-Modular-blueviolet)
![Platform](https://img.shields.io/badge/Platform-Windows-lightgrey)

**STN-LABZ Configurable RAG Creation System**

`rag_builder` is an ISO C application for building structured, traceable knowledge for AI consumption.

The project provides a configurable and modular RAG creation pipeline capable of transforming approved STN-LABZ source material into validated knowledge artifacts suitable for systems such as Digit.

## Purpose

STN-LABZ maintains engineering policies, doctrine, technical documentation, intelligence, and other approved knowledge sources.

`rag_builder` provides a controlled process for converting those sources into retrieval-ready knowledge while preserving source identity, provenance, approval state, and integrity.

The application is intended to support multiple source types and RAG build configurations without coupling the core application to a single project or knowledge domain.

## Initial Scope

Initial development will focus on processing approved STN-LABZ policy documents stored in standard Markdown format.

The initial source repository is a local policy directory.

The first implementation will support:

- Configurable source directories
- Markdown source discovery
- STN-LABZ policy parsing
- Document metadata extraction
- Structured knowledge generation
- Source integrity verification
- Build validation
- Build manifests
- RAG output suitable for Digit consumption

Additional source types and processing capabilities may be introduced through modules as requirements are established.

## Architecture

`rag_builder` uses a modular processing model.

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

The core coordinates the build process while specialized modules perform individual processing functions.

Configuration determines which supported components participate in a particular RAG build.

## Trust and Provenance

Generated knowledge must remain traceable to its authoritative source.

Where applicable, `rag_builder` will preserve or generate information necessary to establish:

- Source document identity
- Source revision
- Approval state
- Source integrity
- Knowledge provenance
- Build identity
- Validation results
- Trust Chain relationships

Generated RAG content does not replace its authoritative source material.

## Human Authority

`rag_builder` does not establish organizational truth or independently authorize source material.

Human-approved source material remains authoritative.

The application transforms authorized information for machine consumption while preserving the distinction between source authority and generated knowledge artifacts.

## Implementation

`rag_builder` is implemented in ISO C.

The project will favor:

- Deterministic processing
- Explicit interfaces
- Modular components
- Configuration-driven behavior
- Traceable transformations
- Fail-safe validation
- Minimal external dependencies

## Status

**Active Development**

Initial architecture and requirements are being established.

## Organization

Developed by **[STN-LABZ](https://www.stn-labz.com)**.

**Engineering systems worthy of trust when trust matters most.**