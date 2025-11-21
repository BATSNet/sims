-- Add 'processing' status to incident table
-- This allows incidents to have a 'processing' state while AI is analyzing them

ALTER TABLE incident DROP CONSTRAINT IF EXISTS incident_status_check;
ALTER TABLE incident ADD CONSTRAINT incident_status_check
  CHECK (status IN ('processing', 'open', 'in_progress', 'resolved', 'closed'));
