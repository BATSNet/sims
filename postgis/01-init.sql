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
    user_phone VARCHAR(13) CHECK (
        user_phone ~ '^(\+?[0-9]{1,12})$'
        AND LENGTH(user_phone) BETWEEN 10 AND 13
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
        CHECK (status IN ('open', 'in_progress', 'resolved', 'closed')),
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
    user_phone VARCHAR(13),
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
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
    incident_id UUID NOT NULL REFERENCES incident(id) ON DELETE CASCADE,
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
