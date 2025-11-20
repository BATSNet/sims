-- Add API integration fields to existing organization table
-- Run this migration to add the new columns

ALTER TABLE organization
ADD COLUMN IF NOT EXISTS api_enabled BOOLEAN DEFAULT false,
ADD COLUMN IF NOT EXISTS api_type VARCHAR(50);

-- Update existing military organizations to enable SEDAP
UPDATE organization
SET api_enabled = true, api_type = 'SEDAP'
WHERE type = 'military';
