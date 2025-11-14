CREATE TABLE IF NOT EXISTS contact (
	id BIGSERIAL PRIMARY KEY,
	user_phone VARCHAR(13) CHECK (
            user_phone ~ '^(\+?[0-9]{1,12})$'   -- only digits, optional leading +
            AND LENGTH(user_phone) BETWEEN  10 AND 13
        ),
	location GEOMETRY(POINT, 4326) NOT NULL,
	time TIMESTAMPTZ NOT NULL DEFAULT NOW(),

	picture_path TEXT,
	video_path TEXT,
    text_description TEXT,

	audio_description_path TEXT,
	audio_transcript TEXT,
	transcript_summary TEXT,

	-- Status: 'alerted', 'escalated', 'deescalated'
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


