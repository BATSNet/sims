# Prompt Evaluation

This directory contains a promptfoo-based evaluation setup for testing incident classification prompts.

## Prerequisites

- Node.js v20 or higher
- npm (comes with Node.js)

## Setup

### 1. Install dependencies

```bash
cd prompt_evaluation
npm install promptfoo
```

Or install globally:

```bash
npm install -g promptfoo
```

### 2. Configure the inference provider

You have two options:

#### Option A: Local inference service (default)

Run a local inference service (e.g., Ollama) on your machine:

```bash
# Start Ollama (example)
ollama serve
```

The default configuration expects the service at `http://localhost:11435`. To change this, edit `promptfooconfig.yaml`:

```yaml
env:
  OLLAMA_BASE_URL: http://localhost:11435

providers:
  - id: ollama:chat:gpt-oss
    config:
      apiBaseUrl: http://localhost:11435
```

#### Option B: External API

To use an external API (e.g., OpenAI, Anthropic), update `promptfooconfig.yaml`:

```yaml
providers:
  - id: openai:gpt-4
    config:
      apiKey: ${OPENAI_API_KEY}
```

Set the API key as an environment variable:

```bash
export OPENAI_API_KEY=your-api-key-here
```

Or for Anthropic:

```yaml
providers:
  - id: anthropic:claude-3-sonnet-20240229
    config:
      apiKey: ${ANTHROPIC_API_KEY}
```

## Running the evaluation

```bash
# Run evaluation
npx promptfoo eval

# View results in browser
npx promptfoo view
```

## Project structure

```
prompt_evaluation/
├── promptfooconfig.yaml    # Main configuration file
├── templates/              # Prompt templates to evaluate
│   └── template*.txt
├── system_prompts/         # System prompts
│   └── system_prompt1.txt
└── README.md
```

## Configuration

The `promptfooconfig.yaml` file contains:

- **prompts**: References to prompt template files
- **providers**: LLM provider configuration
- **defaultTest**: Default assertions and variables for all tests
- **tests**: Individual test cases with incident texts and expected categories

## Adding new test cases

Add new test cases to the `tests` section in `promptfooconfig.yaml`:

```yaml
tests:
  - vars:
      incident_text: "Your incident description here"
      expectedCategory: "expected_category"
```

## Available categories

- drone_detection
- suspicious_vehicle
- suspicious_person
- fire_incident
- medical_emergency
- infrastructure_damage
- cyber_incident
- hazmat_incident
- natural_disaster
- airport_incident
- security_breach
- civil_unrest
- armed_threat
- explosion
- chemical_biological
- maritime_incident
- theft_burglary
- unclassified
