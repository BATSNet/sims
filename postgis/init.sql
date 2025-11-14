-- PostgreSQL initialization script for SIMS
-- This script enables PostGIS and creates the database schema

-- Enable PostGIS extensions
CREATE EXTENSION IF NOT EXISTS postgis;
CREATE EXTENSION IF NOT EXISTS postgis_topology;

-- Verify PostGIS installation
SELECT PostGIS_Version();

-- Create contact table for incident reports
CREATE TABLE IF NOT EXISTS contact (
	id BIGSERIAL PRIMARY KEY,
	user_phone VARCHAR(13) CHECK (
            user_phone ~ '^(\+?[0-9]{1,12})$'   -- only digits, optional leading +
            AND LENGTH(user_phone) BETWEEN  10 AND 13
        ),
	location GEOMETRY(POINT, 4326) NOT NULL,
    date DATE DEFAULT CURRENT_DATE,
    time TIMESTAMPTZ NOT NULL DEFAULT NOW(),

	picture_path TEXT,
	video_path TEXT,
    text_description TEXT,

	audio_description_path TEXT,
	audio_transcript TEXT,
	transcript_summary TEXT,

	-- Status: 'reported', 'escalated', 'deescalated'
	status VARCHAR(20) NOT NULL
		CHECK (status IN ('reported', 'escalated', 'deescalated')),
	-- Where it was routed (organization id)
	routed_to BIGINT,
	category VARCHAR(20) NOT NULL DEFAULT 'Unclassified',
	tags TEXT[] DEFAULT '{}',
	metadata JSONB DEFAULT '{}'::jsonb,

	created_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
	updated_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- Create index on location for spatial queries
CREATE INDEX IF NOT EXISTS idx_contact_location ON contact USING GIST(location);

-- Create index on status for filtering
CREATE INDEX IF NOT EXISTS idx_contact_status ON contact(status);

-- Create index on created_at for time-based queries
CREATE INDEX IF NOT EXISTS idx_contact_created_at ON contact(created_at);
