-- ============================================================================
-- DEMO DATA - Realistic German Emergency Response Organizations
-- ============================================================================
--
-- To disable demo data loading, rename this file (e.g., to 99-demo-data.sql.disabled)
-- or remove it from the postgis directory.
--
-- This file automatically loads when the database is initialized.
-- ============================================================================

-- Military Organizations
INSERT INTO organization (name, short_name, type, contact_person, phone, email, emergency_phone, address, city, country, location, capabilities, response_area, active, notes, api_enabled, api_type) VALUES
('Bundeswehr Einsatzfuhrungszentrum', 'BwEinsFuZent', 'military', 'Oberst Schmidt', '+49 228 12345', 'ops@bundeswehr.de', '+49 228 12300', 'Fontainengraben 150', 'Bonn', 'Germany', ST_SetSRID(ST_MakePoint(7.0982, 50.7374), 4326), ARRAY['air_support', 'ground_forces', 'logistics', 'medical'], 'Nationwide', true, 'Central military operations coordination', true, 'SEDAP'),

('Bundeswehr Kommando Territoriale Aufgaben', 'KdoTerrAufg', 'military', 'Brigadegeneral Mueller', '+49 30 18241500', 'territorial@bundeswehr.de', '+49 30 18241000', 'Invalidenstrasse 35', 'Berlin', 'Germany', ST_SetSRID(ST_MakePoint(13.3777, 52.5200), 4326), ARRAY['disaster_relief', 'infrastructure_protection', 'civil_support'], 'Germany', true, 'Territorial tasks and civil-military cooperation', true, 'SEDAP'),

('Bundeswehr ABC-Abwehrzentrum', 'ABC-AbwZentr', 'military', 'Oberst Dr. Weber', '+49 89 992769-0', 'abc@bundeswehr.de', '+49 89 992769-999', 'Munchen', 'Munchen', 'Germany', ST_SetSRID(ST_MakePoint(11.5820, 48.1351), 4326), ARRAY['cbrn_defense', 'decontamination', 'detection'], 'Nationwide', true, 'NBC defense and hazmat response', true, 'SEDAP'),

-- Police Organizations
('Bundeskriminalamt', 'BKA', 'police', 'Kriminaldirektor Hoffmann', '+49 611 55-0', 'poststelle@bka.bund.de', '+49 611 55-18800', 'Traubenstrasse 21', 'Wiesbaden', 'Germany', ST_SetSRID(ST_MakePoint(8.2397, 50.0782), 4326), ARRAY['investigation', 'counter_terrorism', 'cyber_crime'], 'Nationwide', true, 'Federal Criminal Police Office', false, NULL),

('Bundespolizei Direktion Berlin', 'BPOL Berlin', 'police', 'Polizeidirektor Klein', '+49 30 202920', 'bundespolizei.berlin@polizei.bund.de', '110', 'Invalidenstrasse 92', 'Berlin', 'Germany', ST_SetSRID(ST_MakePoint(13.3777, 52.5200), 4326), ARRAY['border_security', 'airport_security', 'train_security'], 'Berlin-Brandenburg', true, 'Federal Police Berlin', false, NULL),

('Landeskriminalamt Baden-Wurttemberg', 'LKA BW', 'police', 'Kriminaldirektor Fischer', '+49 711 5401-0', 'poststelle@lka.bwl.de', '110', 'Taubenheimstrasse 85', 'Stuttgart', 'Germany', ST_SetSRID(ST_MakePoint(9.1829, 48.7758), 4326), ARRAY['investigation', 'forensics', 'organized_crime'], 'Baden-Wurttemberg', true, 'State Criminal Police Baden-Wurttemberg', false, NULL),

-- Fire Services
('Feuerwehr Berlin', 'BerlinFW', 'fire', 'Branddirektor Wagner', '+49 30 387-11', 'info@berliner-feuerwehr.de', '112', 'Voltairestrasse 2', 'Berlin', 'Germany', ST_SetSRID(ST_MakePoint(13.4050, 52.5200), 4326), ARRAY['fire_fighting', 'technical_rescue', 'hazmat', 'water_rescue'], 'Berlin', true, 'Berlin Fire Department', false, NULL),

('Berufsfeuerwehr Frankfurt', 'FFM-BF', 'fire', 'Branddirektor Zimmermann', '+49 69 212-111', 'feuerwehr@frankfurt.de', '112', 'Hanauer Landstrasse 2', 'Frankfurt', 'Germany', ST_SetSRID(ST_MakePoint(8.6821, 50.1109), 4326), ARRAY['fire_fighting', 'technical_rescue', 'hazmat', 'airport_fire'], 'Frankfurt Region', true, 'Frankfurt Professional Fire Department', false, NULL),

('Feuerwehr Hamburg', 'HH-FW', 'fire', 'Landesbranddirektor Krause', '+49 40 42851-0', 'info@feuerwehr.hamburg.de', '112', 'Westphalensweg 1', 'Hamburg', 'Germany', ST_SetSRID(ST_MakePoint(9.9937, 53.5511), 4326), ARRAY['fire_fighting', 'technical_rescue', 'marine_rescue', 'hazmat'], 'Hamburg', true, 'Hamburg Fire Department', false, NULL),

-- Medical Services
('Deutsches Rotes Kreuz Bundesverband', 'DRK', 'medical', 'Dr. med. Schulz', '+49 30 85404-0', 'drk@drk.de', '+49 30 85404-444', 'Carstennstrasse 58', 'Berlin', 'Germany', ST_SetSRID(ST_MakePoint(13.3777, 52.5200), 4326), ARRAY['emergency_medical', 'disaster_relief', 'blood_service', 'ambulance'], 'Nationwide', true, 'German Red Cross', false, NULL),

('Johanniter-Unfall-Hilfe', 'JUH', 'medical', 'Dr. Koch', '+49 30 26997-0', 'info@johanniter.de', '+49 30 26997-444', 'Lützowstrasse 94', 'Berlin', 'Germany', ST_SetSRID(ST_MakePoint(13.3553, 52.5075), 4326), ARRAY['emergency_medical', 'ambulance', 'air_ambulance', 'patient_transport'], 'Nationwide', true, 'St. John Ambulance Germany', false, NULL),

('Malteser Hilfsdienst', 'MHD', 'medical', 'Dr. Becker', '+49 221 96000-0', 'info@malteser.de', '+49 221 96000-112', 'Kalker Hauptstrasse 22-24', 'Koln', 'Germany', ST_SetSRID(ST_MakePoint(6.9603, 50.9375), 4326), ARRAY['emergency_medical', 'ambulance', 'disaster_relief', 'elderly_care'], 'Nationwide', true, 'Malteser Aid Service', false, NULL),

('ADAC Luftrettung', 'ADAC LR', 'medical', 'Flugkapitan Richter', '+49 89 76765-0', 'luftrettung@adac.de', '+49 89 76765-112', 'Am Blutenburgerwaldchen 4', 'Munchen', 'Germany', ST_SetSRID(ST_MakePoint(11.5167, 48.1620), 4326), ARRAY['air_ambulance', 'mountain_rescue', 'emergency_medical'], 'Germany', true, 'ADAC Air Rescue', false, NULL),

-- Civil Defense
('Bundesamt fur Bevolkerungsschutz und Katastrophenhilfe', 'BBK', 'civil_defense', 'Dr. Lehmann', '+49 228 99550-0', 'poststelle@bbk.bund.de', '+49 228 99550-112', 'Provinzialstrasse 93', 'Bonn', 'Germany', ST_SetSRID(ST_MakePoint(7.0982, 50.7374), 4326), ARRAY['civil_protection', 'disaster_management', 'warning_systems', 'crisis_management'], 'Nationwide', true, 'Federal Office of Civil Protection and Disaster Assistance', false, NULL),

('Technisches Hilfswerk', 'THW', 'civil_defense', 'Dipl.-Ing. Neumann', '+49 228 940-0', 'thw@thw.de', '+49 228 940-1333', 'Provinzialstrasse 93', 'Bonn', 'Germany', ST_SetSRID(ST_MakePoint(7.0982, 50.7374), 4326), ARRAY['technical_rescue', 'infrastructure', 'water_supply', 'power_generation'], 'Nationwide', true, 'Federal Agency for Technical Relief', false, NULL),

('THW Ortsverband Berlin Mitte', 'THW BE-Mitte', 'civil_defense', 'Zugfuhrer Hartmann', '+49 30 34709-0', 'ov-berlin-mitte@thw.de', '+49 30 34709-999', 'Seestrasse 74', 'Berlin', 'Germany', ST_SetSRID(ST_MakePoint(13.3777, 52.5200), 4326), ARRAY['technical_rescue', 'debris_removal', 'shoring', 'pumping'], 'Berlin Central', true, 'THW Local Unit Berlin Center', false, NULL),

-- Government
('Lagezentrum Bundesministerium des Innern', 'BMI-LZ', 'government', 'Ministerialrat Meier', '+49 30 18681-0', 'lagezentrum@bmi.bund.de', '+49 30 18681-999', 'Alt-Moabit 140', 'Berlin', 'Germany', ST_SetSRID(ST_MakePoint(13.3483, 52.5233), 4326), ARRAY['situation_center', 'coordination', 'crisis_management'], 'Federal', true, 'Federal Ministry of Interior Situation Center', false, NULL),

('Gemeinsames Melde- und Lagezentrum Berlin', 'GMLZ Berlin', 'government', 'Oberbrandrat Wolf', '+49 30 90129-6600', 'gmlz@seninnds.berlin.de', '+49 30 90129-6666', 'Voltairestrasse 2', 'Berlin', 'Germany', ST_SetSRID(ST_MakePoint(13.4050, 52.5200), 4326), ARRAY['coordination', 'situation_awareness', 'multi_agency'], 'Berlin', true, 'Joint Reporting and Situation Center Berlin', false, NULL),

-- Airport Authorities
('Flughafen Berlin Brandenburg', 'BER', 'other', 'Sicherheitsdirektor Schwarz', '+49 30 609170', 'info@berlin-airport.de', '+49 30 609170-112', 'Willy-Brandt-Platz', 'Schonefeld', 'Germany', ST_SetSRID(ST_MakePoint(13.5033, 52.3667), 4326), ARRAY['airport_security', 'fire_fighting', 'medical'], 'Berlin Airport', true, 'Berlin Brandenburg Airport Authority', false, NULL),

('Flughafen Frankfurt', 'FRA', 'other', 'Leiter Werkfeuerwehr Keller', '+49 69 690-0', 'security@fraport.de', '+49 69 690-77110', 'Frankfurt Airport', 'Frankfurt', 'Germany', ST_SetSRID(ST_MakePoint(8.5622, 50.0379), 4326), ARRAY['airport_security', 'fire_fighting', 'hazmat', 'medical'], 'Frankfurt Airport', true, 'Frankfurt Airport Authority', false, NULL);

-- ============================================================================
-- DEMO INTEGRATIONS - Auto-assign BMS to Bundeswehr Organizations
-- ============================================================================

-- Assign SEDAP BMS Integration to all military organizations
INSERT INTO organization_integration (organization_id, template_id, name, description, config, auth_credentials, trigger_filters, active, created_by)
SELECT
    o.id,
    t.id,
    'BMS Integration',
    'Automatic SEDAP integration for Battle Management System',
    '{"endpoint_url": "https://sedap.example.com/api/incidents", "timeout": 30}'::jsonb,
    '{}'::jsonb,
    '{"priorities": ["critical", "high"]}'::jsonb,
    true,
    'demo_seed'
FROM organization o
CROSS JOIN integration_template t
WHERE o.type = 'military'
AND t.name = 'SEDAP BMS Integration'
ON CONFLICT DO NOTHING;
