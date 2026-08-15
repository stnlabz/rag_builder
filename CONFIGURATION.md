# Configuration

## Purpose

`rag_builder` is configuration-driven. Configuration determines source locations, enabled pipeline modules, validation behavior, and output destinations.

## Initial Source

The first supported source type is a local directory containing STN-LABZ Markdown policy documents.

Initial development source:

```text
C:\Users\Poe\Desktop\STN-Labz\Policies
```

The path is a development default and will remain configurable.

## Planned Configuration Areas

Configuration will support, as requirements are implemented:

- Source type
- Source path
- Accepted file extensions
- Enabled parser
- Enabled classifier
- Enabled chunker
- Metadata processing
- Relationship processing
- Trust Chain validation
- Deduplication
- Conflict detection
- Output format
- Output path
- Logging behavior

## Configuration Rules

Configuration will be validated before a build begins.

Invalid or unsupported configuration will cause the build to stop with an explicit error rather than silently fall back to unknown behavior.

Local machine configuration should not be committed when it contains environment-specific or sensitive values.
