-- PostgreSQL initialization script for SIMS
-- This script enables PostGIS and creates the database schema

-- Enable PostGIS extensions
CREATE EXTENSION IF NOT EXISTS postgis;
CREATE EXTENSION IF NOT EXISTS postgis_topology;
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- Verify PostGIS installation
SELECT PostGIS_Version();

-- ============================================================================
-- ORGANIZATION TABLE
-- ============================================================================
CREATE TABLE IF NOT EXISTS organization (
    id BIGSERIAL PRIMARY KEY,
    name VARCHAR(200) NOT NULL,
    short_name VARCHAR(100),
    type VARCHAR(50) NOT NULL CHECK (type IN ('military', 'police', 'fire', 'medical', 'civil_defense', 'government', 'other')),

    -- Contact information
    contact_person VARCHAR(200),
    phone VARCHAR(20),
    email VARCHAR(100),
    emergency_phone VARCHAR(20),

    -- Address
    address TEXT,
    city VARCHAR(100),
    country VARCHAR(100) DEFAULT 'Germany',

    -- Location
    location GEOMETRY(POINT, 4326),

    -- Capabilities and metadata
    capabilities TEXT[] DEFAULT '{}',
    response_area TEXT,
    active BOOLEAN DEFAULT true,
    notes TEXT,

    -- Additional contact info as JSONB for flexibility
    additional_contacts JSONB DEFAULT '[]'::jsonb,

    -- External API Integration (SEDAP, KATWARN, etc.)
    -- Actual endpoint configs are stored in config files
    api_enabled BOOLEAN DEFAULT false,
    api_type VARCHAR(50),

    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Organization indexes
CREATE INDEX IF NOT EXISTS idx_organization_type ON organization(type);
CREATE INDEX IF NOT EXISTS idx_organization_active ON organization(active);
CREATE INDEX IF NOT EXISTS idx_organization_location ON organization USING GIST(location);

-- ============================================================================
-- INCIDENT TABLE (formerly contact)
-- ============================================================================
CREATE TABLE IF NOT EXISTS incident (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    incident_id VARCHAR(50) UNIQUE NOT NULL,
    user_phone VARCHAR(20) CHECK (
        user_phone ~ '^(\+?[0-9]{1,18})$'
        AND LENGTH(user_phone) BETWEEN 10 AND 20
    ),

    -- Location data
    location GEOMETRY(POINT, 4326),
    latitude DOUBLE PRECISION,
    longitude DOUBLE PRECISION,
    heading DOUBLE PRECISION,

    -- Incident details
    title TEXT,
    description TEXT,

    -- Status and priority (aligned with Flutter)
    status VARCHAR(20) NOT NULL DEFAULT 'open'
        CHECK (status IN ('processing', 'open', 'in_progress', 'resolved', 'closed')),
    priority VARCHAR(20) NOT NULL DEFAULT 'medium'
        CHECK (priority IN ('critical', 'high', 'medium', 'low')),
    category VARCHAR(50) NOT NULL DEFAULT 'Unclassified',

    -- Routing
    routed_to BIGINT REFERENCES organization(id),

    -- Metadata
    tags TEXT[] DEFAULT '{}',
    metadata JSONB DEFAULT '{}'::jsonb,

    -- Timestamps
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Incident indexes
CREATE INDEX IF NOT EXISTS idx_incident_location ON incident USING GIST(location);
CREATE INDEX IF NOT EXISTS idx_incident_status ON incident(status);
CREATE INDEX IF NOT EXISTS idx_incident_priority ON incident(priority);
CREATE INDEX IF NOT EXISTS idx_incident_created_at ON incident(created_at);
CREATE INDEX IF NOT EXISTS idx_incident_incident_id ON incident(incident_id);

-- ============================================================================
-- CHAT SESSION TABLE
-- ============================================================================
CREATE TABLE IF NOT EXISTS chat_session (
    id BIGSERIAL PRIMARY KEY,
    session_id UUID UNIQUE NOT NULL DEFAULT uuid_generate_v4(),
    incident_id UUID NOT NULL REFERENCES incident(id) ON DELETE CASCADE,
    user_phone VARCHAR(20),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    last_modified TIMESTAMPTZ NOT NULL DEFAULT NOW(),

    summary TEXT,
    last_summarized TIMESTAMPTZ
);

-- Chat session indexes
CREATE INDEX IF NOT EXISTS idx_chat_session_incident_id ON chat_session(incident_id);
CREATE INDEX IF NOT EXISTS idx_chat_session_session_id ON chat_session(session_id);

-- ============================================================================
-- CHAT MESSAGE TABLE (langchain-compatible format)
-- ============================================================================
CREATE TABLE IF NOT EXISTS chat_message (
    id BIGSERIAL PRIMARY KEY,
    session_id UUID NOT NULL REFERENCES chat_session(session_id) ON DELETE CASCADE,
    message JSONB NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Chat message indexes
CREATE INDEX IF NOT EXISTS idx_chat_message_session_id ON chat_message(session_id);
CREATE INDEX IF NOT EXISTS idx_chat_message_created_at ON chat_message(created_at);

-- ============================================================================
-- MEDIA TABLE
-- ============================================================================
CREATE TABLE IF NOT EXISTS media (
    id UUID PRIMARY KEY DEFAULT uuid_generate_v4(),
    incident_id UUID REFERENCES incident(id) ON DELETE CASCADE,
    chat_message_id BIGINT REFERENCES chat_message(id) ON DELETE SET NULL,

    -- File information
    file_path TEXT NOT NULL,
    file_url TEXT NOT NULL,
    mime_type TEXT NOT NULL,
    file_size BIGINT,
    media_type VARCHAR(20) CHECK (media_type IN ('image', 'audio', 'video')),

    -- Transcription for audio files
    transcription TEXT,

    -- Metadata
    metadata JSONB DEFAULT '{}'::jsonb,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Media indexes
CREATE INDEX IF NOT EXISTS idx_media_incident_id ON media(incident_id);
CREATE INDEX IF NOT EXISTS idx_media_message_id ON media(chat_message_id);
CREATE INDEX IF NOT EXISTS idx_media_type ON media(media_type);

-- ============================================================================
-- ORGANIZATION TOKENS TABLE (for responder portal access)
-- ============================================================================
CREATE TABLE IF NOT EXISTS organization_tokens (
    id BIGSERIAL PRIMARY KEY,
    organization_id BIGINT NOT NULL REFERENCES organization(id) ON DELETE CASCADE,
    token VARCHAR(255) UNIQUE NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    expires_at TIMESTAMPTZ,
    created_by VARCHAR(100),
    last_used_at TIMESTAMPTZ,
    active BOOLEAN DEFAULT true
);

-- Organization tokens indexes
CREATE INDEX IF NOT EXISTS idx_org_tokens_organization_id ON organization_tokens(organization_id);
CREATE INDEX IF NOT EXISTS idx_org_tokens_token ON organization_tokens(token);
CREATE INDEX IF NOT EXISTS idx_org_tokens_active ON organization_tokens(active);

-- ============================================================================
-- INCIDENT NOTES TABLE (internal responder notes, not visible to reporter)
-- ============================================================================
CREATE TABLE IF NOT EXISTS incident_notes (
    id BIGSERIAL PRIMARY KEY,
    incident_id UUID NOT NULL REFERENCES incident(id) ON DELETE CASCADE,
    organization_id BIGINT NOT NULL REFERENCES organization(id) ON DELETE CASCADE,
    note_text TEXT NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    created_by VARCHAR(100)
);

-- Incident notes indexes
CREATE INDEX IF NOT EXISTS idx_incident_notes_incident_id ON incident_notes(incident_id);
CREATE INDEX IF NOT EXISTS idx_incident_notes_organization_id ON incident_notes(organization_id);
CREATE INDEX IF NOT EXISTS idx_incident_notes_created_at ON incident_notes(created_at);

-- ============================================================================
-- HELPER FUNCTIONS
-- ============================================================================

-- Function to update updated_at timestamp
CREATE OR REPLACE FUNCTION update_updated_at_column()
RETURNS TRIGGER AS $$
BEGIN
    NEW.updated_at = NOW();
    RETURN NEW;
END;
$$ language 'plpgsql';

-- Trigger to auto-update updated_at on incident
CREATE TRIGGER update_incident_updated_at BEFORE UPDATE ON incident
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- Trigger to auto-update updated_at on organization
CREATE TRIGGER update_organization_updated_at BEFORE UPDATE ON organization
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- INTEGRATION TEMPLATE TABLE
-- Admin-managed templates defining available integration types
-- ============================================================================
CREATE TABLE IF NOT EXISTS integration_template (
    id BIGSERIAL PRIMARY KEY,
    name VARCHAR(100) UNIQUE NOT NULL,
    type VARCHAR(50) NOT NULL CHECK (type IN ('webhook', 'sedap', 'email', 'sms', 'whatsapp', 'n8n', 'custom')),
    description TEXT,

    -- Configuration schema defines what fields are required/optional
    -- Example: {"endpoint_url": {"type": "string", "required": true}, "timeout": {"type": "integer", "default": 30}}
    config_schema JSONB NOT NULL DEFAULT '{}'::jsonb,

    -- Default payload template (Jinja2 format for webhook, email body, etc.)
    -- Variables: {{incident.id}}, {{incident.title}}, {{incident.latitude}}, etc.
    payload_template TEXT,

    -- Authentication configuration
    auth_type VARCHAR(50) NOT NULL DEFAULT 'none'
        CHECK (auth_type IN ('none', 'bearer_token', 'api_key', 'basic_auth', 'oauth2', 'custom_header')),
    auth_schema JSONB DEFAULT '{}'::jsonb,

    -- Delivery settings
    timeout_seconds BIGINT DEFAULT 30,
    retry_enabled BOOLEAN DEFAULT false,
    retry_attempts BIGINT DEFAULT 0,

    -- Metadata
    active BOOLEAN DEFAULT true,
    system_template BOOLEAN DEFAULT false,  -- True for built-in templates (SEDAP)

    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    created_by VARCHAR(100)
);

-- Integration template indexes
CREATE INDEX IF NOT EXISTS idx_integration_template_type ON integration_template(type);
CREATE INDEX IF NOT EXISTS idx_integration_template_active ON integration_template(active);

-- Trigger to auto-update updated_at
CREATE TRIGGER update_integration_template_updated_at BEFORE UPDATE ON integration_template
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- ORGANIZATION INTEGRATION TABLE
-- Organization-specific integration instances
-- ============================================================================
CREATE TABLE IF NOT EXISTS organization_integration (
    id BIGSERIAL PRIMARY KEY,
    organization_id BIGINT NOT NULL REFERENCES organization(id) ON DELETE CASCADE,
    template_id BIGINT NOT NULL REFERENCES integration_template(id) ON DELETE CASCADE,

    -- Custom name for this integration instance
    name VARCHAR(200) NOT NULL,
    description TEXT,

    -- Configuration values (validated against template's config_schema)
    -- Example: {"endpoint_url": "https://org.example.com/webhook", "timeout": 60}
    config JSONB NOT NULL DEFAULT '{}'::jsonb,

    -- Authentication credentials (should be encrypted at rest in production)
    -- Example: {"token": "encrypted_bearer_token"} or {"username": "user", "password": "encrypted_pass"}
    auth_credentials JSONB DEFAULT '{}'::jsonb,

    -- Custom payload template override (optional, uses template default if null)
    custom_payload_template TEXT,

    -- Filters for when to trigger this integration
    -- Example: {"priorities": ["critical", "high"], "categories": ["Security", "Military"]}
    trigger_filters JSONB DEFAULT '{}'::jsonb,

    -- Status
    active BOOLEAN DEFAULT true,
    last_delivery_at TIMESTAMPTZ,
    last_delivery_status VARCHAR(20),

    -- Metadata
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    created_by VARCHAR(100)
);

-- Organization integration indexes
CREATE INDEX IF NOT EXISTS idx_org_integration_org_id ON organization_integration(organization_id);
CREATE INDEX IF NOT EXISTS idx_org_integration_template_id ON organization_integration(template_id);
CREATE INDEX IF NOT EXISTS idx_org_integration_active ON organization_integration(active);

-- Trigger to auto-update updated_at
CREATE TRIGGER update_organization_integration_updated_at BEFORE UPDATE ON organization_integration
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- INTEGRATION DELIVERY TABLE
-- Tracks all delivery attempts for auditing and retry
-- ============================================================================
CREATE TABLE IF NOT EXISTS integration_delivery (
    id BIGSERIAL PRIMARY KEY,
    incident_id UUID NOT NULL REFERENCES incident(id) ON DELETE CASCADE,
    organization_id BIGINT NOT NULL REFERENCES organization(id) ON DELETE CASCADE,
    integration_id BIGINT REFERENCES organization_integration(id) ON DELETE SET NULL,

    -- Integration details (snapshot at delivery time)
    integration_type VARCHAR(50) NOT NULL,
    integration_name VARCHAR(200),

    -- Delivery details
    status VARCHAR(20) NOT NULL DEFAULT 'pending'
        CHECK (status IN ('pending', 'success', 'failed', 'timeout', 'retrying')),
    attempt_number INTEGER DEFAULT 1,

    -- Request/response data
    request_payload JSONB,
    request_url TEXT,
    response_code INTEGER,
    response_body TEXT,
    error_message TEXT,

    -- Timing
    started_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    completed_at TIMESTAMPTZ,
    duration_ms INTEGER,

    -- Metadata
    metadata JSONB DEFAULT '{}'::jsonb
);

-- Integration delivery indexes
CREATE INDEX IF NOT EXISTS idx_integration_delivery_incident_id ON integration_delivery(incident_id);
CREATE INDEX IF NOT EXISTS idx_integration_delivery_org_id ON integration_delivery(organization_id);
CREATE INDEX IF NOT EXISTS idx_integration_delivery_integration_id ON integration_delivery(integration_id);
CREATE INDEX IF NOT EXISTS idx_integration_delivery_status ON integration_delivery(status);
CREATE INDEX IF NOT EXISTS idx_integration_delivery_started_at ON integration_delivery(started_at);

-- ============================================================================
-- INBOUND WEBHOOK TABLE
-- Manages webhook endpoints for receiving external incidents
-- ============================================================================
CREATE TABLE IF NOT EXISTS inbound_webhook (
    id BIGSERIAL PRIMARY KEY,

    -- Webhook identity
    name VARCHAR(200) NOT NULL,
    description TEXT,
    webhook_token VARCHAR(100) UNIQUE NOT NULL,

    -- Security
    auth_token VARCHAR(255) NOT NULL,
    allowed_ips JSONB DEFAULT '[]'::jsonb,

    -- Source information
    source_name VARCHAR(200),
    source_type VARCHAR(50),

    -- Payload transformation
    -- Maps external payload structure to SIMS incident format
    -- Example: {"title": "$.message.text", "latitude": "$.location.lat", "longitude": "$.location.lng"}
    field_mapping JSONB NOT NULL DEFAULT '{}'::jsonb,

    -- Default values for created incidents
    default_values JSONB DEFAULT '{}'::jsonb,

    -- Auto-assignment
    auto_assign_to_org BIGINT REFERENCES organization(id) ON DELETE SET NULL,

    -- Status and metrics
    active BOOLEAN DEFAULT true,
    total_received BIGINT DEFAULT 0,
    last_received_at TIMESTAMPTZ,

    -- Metadata
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
    created_by VARCHAR(100)
);

-- Inbound webhook indexes
CREATE INDEX IF NOT EXISTS idx_inbound_webhook_token ON inbound_webhook(webhook_token);
CREATE INDEX IF NOT EXISTS idx_inbound_webhook_active ON inbound_webhook(active);
CREATE INDEX IF NOT EXISTS idx_inbound_webhook_source_type ON inbound_webhook(source_type);

-- Trigger to auto-update updated_at
CREATE TRIGGER update_inbound_webhook_updated_at BEFORE UPDATE ON inbound_webhook
    FOR EACH ROW EXECUTE FUNCTION update_updated_at_column();

-- ============================================================================
-- SEED DEFAULT INTEGRATION TEMPLATES
-- ============================================================================

-- Generic Webhook Template
INSERT INTO integration_template (name, type, description, config_schema, payload_template, auth_type, system_template, created_by)
VALUES (
    'Generic Webhook',
    'webhook',
    'Generic HTTP POST webhook that can integrate with any system (Zapier, Make, n8n, custom endpoints)',
    '{"endpoint_url": {"type": "string", "required": true, "description": "Full webhook URL"}, "timeout": {"type": "integer", "default": 30, "description": "Request timeout in seconds"}, "custom_headers": {"type": "object", "default": {}, "description": "Additional HTTP headers"}}'::jsonb,
    '{
  "incident_id": "{{incident.incident_id}}",
  "title": "{{incident.title}}",
  "description": "{{incident.description}}",
  "priority": "{{incident.priority}}",
  "status": "{{incident.status}}",
  "category": "{{incident.category}}",
  "location": {
    "latitude": {{incident.latitude}},
    "longitude": {{incident.longitude}},
    "heading": {{incident.heading}}
  },
  "created_at": "{{incident.created_at}}",
  "user_phone": "{{incident.user_phone}}",
  "tags": {{incident.tags | tojson}},
  "media": {
    "image_url": "{{incident.image_url}}",
    "video_url": "{{incident.video_url}}",
    "audio_url": "{{incident.audio_url}}",
    "audio_transcript": "{{incident.audio_transcript}}"
  }
}',
    'bearer_token',
    false,
    'system'
) ON CONFLICT (name) DO NOTHING;

-- SEDAP Template
INSERT INTO integration_template (name, type, description, config_schema, payload_template, auth_type, system_template, created_by)
VALUES (
    'SEDAP BMS Integration',
    'sedap',
    'SEDAP.Express integration for Battle Management Systems (BMS) using CSV over REST',
    '{"endpoint_url": {"type": "string", "required": true, "description": "SEDAP.Express API endpoint"}, "timeout": {"type": "integer", "default": 30}}'::jsonb,
    NULL,  -- SEDAP uses custom CSV format, not Jinja2
    'none',
    true,
    'system'
) ON CONFLICT (name) DO NOTHING;

-- Email Template
INSERT INTO integration_template (name, type, description, config_schema, payload_template, auth_type, system_template, created_by)
VALUES (
    'Email Notification',
    'email',
    'Send incident alerts via email',
    '{"smtp_host": {"type": "string", "required": true}, "smtp_port": {"type": "integer", "default": 587}, "from_email": {"type": "string", "required": true}, "to_emails": {"type": "array", "required": true, "description": "List of recipient email addresses"}}'::jsonb,
    'Subject: [SIMS] {{incident.priority | upper}} - {{incident.title}}

Incident ID: {{incident.incident_id}}
Priority: {{incident.priority}}
Category: {{incident.category}}
Status: {{incident.status}}

Description:
{{incident.description}}

Location:
Latitude: {{incident.latitude}}
Longitude: {{incident.longitude}}
Heading: {{incident.heading}}

Reported by: {{incident.user_phone}}
Reported at: {{incident.created_at}}

{% if incident.image_url %}
Image: {{incident.image_url}}
{% endif %}
{% if incident.audio_url %}
Audio: {{incident.audio_url}}
{% endif %}
{% if incident.audio_transcript %}
Audio Transcript: {{incident.audio_transcript}}
{% endif %}

Tags: {{incident.tags | join(", ")}}
',
    'basic_auth',
    false,
    'system'
) ON CONFLICT (name) DO NOTHING;
