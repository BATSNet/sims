#!/usr/bin/env python3
"""
SEDAP Message Validator

Validates SEDAP CONTACT and TEXT messages against ICD v1.0 specification.
Fetches incidents from database, generates SEDAP messages, and validates each field.
"""
import sys
import os
import base64
import re
from datetime import datetime

# Add parent directory to path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker


# SEDAP ICD v1.0 Field Specifications
VALID_CLASSIFICATIONS = ['P', 'U', 'R', 'C', 'S', 'T']
VALID_SOURCES = ['R', 'A', 'I', 'S', 'E', 'O', 'Y', 'M']  # Radar, AIS, IFF, Sonar, EW, Optical, Synthetic, Manual
VALID_TEXT_TYPES = ['', '0', '1', '2', '3', '4']  # 0=Undefined, 1=Alert, 2=Warning, 3=Notice, 4=Chat
VALID_ENCODINGS = ['BASE64', 'NONE', '']
VALID_DELETE_FLAGS = ['TRUE', 'FALSE', '']


class ValidationResult:
    def __init__(self):
        self.errors = []
        self.warnings = []
        self.info = []

    def add_error(self, field: str, msg: str):
        self.errors.append(f"[ERROR] {field}: {msg}")

    def add_warning(self, field: str, msg: str):
        self.warnings.append(f"[WARN] {field}: {msg}")

    def add_info(self, field: str, msg: str):
        self.info.append(f"[INFO] {field}: {msg}")

    def is_valid(self) -> bool:
        return len(self.errors) == 0

    def print_results(self):
        for msg in self.info:
            print(f"  {msg}")
        for msg in self.warnings:
            print(f"  {msg}")
        for msg in self.errors:
            print(f"  {msg}")


def validate_hex_timestamp(value: str, field_name: str, result: ValidationResult):
    """Validate 64-bit Unix timestamp in hex (milliseconds)."""
    if not value:
        result.add_warning(field_name, "Empty timestamp")
        return

    try:
        ts_ms = int(value, 16)
        # Should be a reasonable timestamp (between 2020 and 2030)
        min_ts = 1577836800000  # 2020-01-01
        max_ts = 1893456000000  # 2030-01-01

        if ts_ms < min_ts or ts_ms > max_ts:
            result.add_warning(field_name, f"Timestamp {ts_ms} seems out of expected range")
        else:
            dt = datetime.fromtimestamp(ts_ms / 1000)
            result.add_info(field_name, f"Valid timestamp: {dt.isoformat()}")
    except ValueError:
        result.add_error(field_name, f"Invalid hex timestamp: {value}")


def validate_classification(value: str, result: ValidationResult):
    """Validate classification field."""
    if not value:
        result.add_info("Classification", "Empty (optional)")
        return

    if value not in VALID_CLASSIFICATIONS:
        result.add_error("Classification", f"Invalid value '{value}'. Must be one of: {VALID_CLASSIFICATIONS}")
    else:
        result.add_info("Classification", f"Valid: {value}")


def validate_latitude(value: str, result: ValidationResult):
    """Validate latitude in decimal degrees."""
    if not value:
        result.add_error("Latitude", "Missing (mandatory)")
        return

    try:
        lat = float(value)
        if lat < -90 or lat > 90:
            result.add_error("Latitude", f"Out of range: {lat}. Must be -90 to 90")
        else:
            result.add_info("Latitude", f"Valid: {lat}")
    except ValueError:
        result.add_error("Latitude", f"Invalid number: {value}")


def validate_longitude(value: str, result: ValidationResult):
    """Validate longitude in decimal degrees."""
    if not value:
        result.add_error("Longitude", "Missing (mandatory)")
        return

    try:
        lon = float(value)
        if lon < -180 or lon > 180:
            result.add_error("Longitude", f"Out of range: {lon}. Must be -180 to 180")
        else:
            result.add_info("Longitude", f"Valid: {lon}")
    except ValueError:
        result.add_error("Longitude", f"Invalid number: {value}")


def validate_sidc(value: str, result: ValidationResult):
    """Validate Symbol Identification Code (MIL-STD-2525)."""
    if not value:
        result.add_info("SIDC", "Empty (optional)")
        return

    # SIDC should be 15 characters for MIL-STD-2525B/C
    if len(value) != 15:
        result.add_warning("SIDC", f"Length is {len(value)}, expected 15 characters")

    # Basic format check: should be alphanumeric and dashes
    if not re.match(r'^[A-Za-z0-9\-]+$', value):
        result.add_error("SIDC", f"Invalid characters in SIDC: {value}")
    else:
        result.add_info("SIDC", f"Valid format: {value}")


def validate_source(value: str, result: ValidationResult):
    """Validate contact source field."""
    if not value:
        result.add_info("Source", "Empty (optional)")
        return

    # Source can contain multiple values
    for char in value:
        if char not in VALID_SOURCES:
            result.add_error("Source", f"Invalid source type '{char}'. Must be one of: {VALID_SOURCES}")
            return

    result.add_info("Source", f"Valid: {value}")


def validate_base64_field(value: str, field_name: str, max_bytes: int, result: ValidationResult):
    """Validate BASE64 encoded field."""
    if not value:
        result.add_info(field_name, "Empty (optional)")
        return

    try:
        decoded = base64.b64decode(value)
        if len(decoded) > max_bytes:
            result.add_warning(field_name, f"Decoded length {len(decoded)} exceeds recommended {max_bytes} bytes")
        else:
            preview = decoded[:50].decode('utf-8', errors='replace')
            result.add_info(field_name, f"Valid BASE64, {len(decoded)} bytes. Preview: {preview}...")
    except Exception as e:
        result.add_error(field_name, f"Invalid BASE64: {e}")


def validate_delete_flag(value: str, result: ValidationResult):
    """Validate delete flag field."""
    if value not in VALID_DELETE_FLAGS:
        result.add_error("DeleteFlag", f"Invalid value '{value}'. Must be TRUE, FALSE, or empty")
    else:
        result.add_info("DeleteFlag", f"Valid: {value or '(empty)'}")


def validate_text_type(value: str, result: ValidationResult):
    """Validate TEXT message type field."""
    if value not in VALID_TEXT_TYPES:
        result.add_error("Type", f"Invalid value '{value}'. Must be one of: {VALID_TEXT_TYPES}")
    else:
        type_names = {'': 'Undefined', '0': 'Undefined', '1': 'Alert', '2': 'Warning', '3': 'Notice', '4': 'Chat'}
        result.add_info("Type", f"Valid: {value} ({type_names.get(value, 'Unknown')})")


def validate_encoding(value: str, result: ValidationResult):
    """Validate encoding field."""
    if value not in VALID_ENCODINGS:
        result.add_error("Encoding", f"Invalid value '{value}'. Must be BASE64, NONE, or empty")
    else:
        result.add_info("Encoding", f"Valid: {value or '(empty)'}")


def validate_contact_message(message: str) -> ValidationResult:
    """
    Validate SEDAP CONTACT message.

    Structure per ICD v1.0:
    CONTACT;<Number>;<Time>;<Sender>;<Classification>;<Acknowledgement>;<MAC>;
    <ContactID>(M);<DeleteFlag>;<Latitude>(M);<Longitude>(M);<Altitude>;
    <relX>;<relY>;<relZ>;<Speed>;<Course>;
    <Heading>;<Roll>;<Pitch>;<Width>;<Length>;<Height>;
    <Name>;<Source>;<SIDC>;<MMSI>;<ICAO>;<Image>;<Comment>
    """
    result = ValidationResult()

    # Remove trailing newline/carriage return
    message = message.rstrip('\r\n')

    # Split by semicolon
    parts = message.split(';')

    # Check message type
    if parts[0] != 'CONTACT':
        result.add_error("MessageType", f"Expected 'CONTACT', got '{parts[0]}'")
        return result

    result.add_info("MessageType", "CONTACT")

    # Expected field count: 29 fields (index 0-28)
    expected_fields = 29
    if len(parts) < expected_fields:
        result.add_warning("FieldCount", f"Got {len(parts)} fields, expected {expected_fields}")

    # Pad with empty strings if needed
    while len(parts) < expected_fields:
        parts.append('')

    # Field indices per ICD spec
    idx = {
        'Number': 1,
        'Time': 2,
        'Sender': 3,
        'Classification': 4,
        'Acknowledgement': 5,
        'MAC': 6,
        'ContactID': 7,
        'DeleteFlag': 8,
        'Latitude': 9,
        'Longitude': 10,
        'Altitude': 11,
        'RelX': 12,
        'RelY': 13,
        'RelZ': 14,
        'Speed': 15,
        'Course': 16,
        'Heading': 17,
        'Roll': 18,
        'Pitch': 19,
        'Width': 20,
        'Length': 21,
        'Height': 22,
        'Name': 23,
        'Source': 24,
        'SIDC': 25,
        'MMSI': 26,
        'ICAO': 27,
        'Image': 28,
        'Comment': 29 if len(parts) > 29 else 28
    }

    # Adjust for actual message structure (Comment might be last field)
    if len(parts) >= 29:
        idx['Comment'] = 28

    # Validate each field
    result.add_info("Number", f"Value: '{parts[idx['Number']]}'")
    validate_hex_timestamp(parts[idx['Time']], "Time", result)
    result.add_info("Sender", f"Value: '{parts[idx['Sender']]}'")
    validate_classification(parts[idx['Classification']], result)
    result.add_info("Acknowledgement", f"Value: '{parts[idx['Acknowledgement']]}'")
    result.add_info("MAC", f"Value: '{parts[idx['MAC']]}'")

    # ContactID is mandatory
    if not parts[idx['ContactID']]:
        result.add_error("ContactID", "Missing (mandatory)")
    else:
        result.add_info("ContactID", f"Value: '{parts[idx['ContactID']]}'")

    validate_delete_flag(parts[idx['DeleteFlag']], result)
    validate_latitude(parts[idx['Latitude']], result)
    validate_longitude(parts[idx['Longitude']], result)
    result.add_info("Altitude", f"Value: '{parts[idx['Altitude']]}'")

    # Name max 64 bytes
    name = parts[idx['Name']]
    if len(name) > 64:
        result.add_error("Name", f"Length {len(name)} exceeds 64 bytes")
    else:
        result.add_info("Name", f"Value: '{name}'")

    validate_source(parts[idx['Source']], result)
    validate_sidc(parts[idx['SIDC']], result)
    result.add_info("MMSI", f"Value: '{parts[idx['MMSI']]}'")
    result.add_info("ICAO", f"Value: '{parts[idx['ICAO']]}'")

    # Image: BASE64, preferred length up to 65000 bytes
    if len(parts) > idx['Image']:
        validate_base64_field(parts[idx['Image']], "Image", 65000, result)

    # Comment: BASE64, max 2048 bytes
    if len(parts) > idx['Comment']:
        validate_base64_field(parts[idx['Comment']], "Comment", 2048, result)

    return result


def validate_text_message(message: str) -> ValidationResult:
    """
    Validate SEDAP TEXT message.

    Structure per ICD v1.0:
    TEXT;<Number>;<Time>;<Sender>;<Classification>;<Acknowledgement>;<MAC>;
    <Recipient>;<Type>;<Encoding>;<Text>(M)
    """
    result = ValidationResult()

    # Remove trailing newline/carriage return
    message = message.rstrip('\r\n')

    # Split by semicolon
    parts = message.split(';')

    # Check message type
    if parts[0] != 'TEXT':
        result.add_error("MessageType", f"Expected 'TEXT', got '{parts[0]}'")
        return result

    result.add_info("MessageType", "TEXT")

    # Expected field count: 11 fields (index 0-10)
    expected_fields = 11
    if len(parts) < expected_fields:
        result.add_warning("FieldCount", f"Got {len(parts)} fields, expected {expected_fields}")

    # Pad with empty strings if needed
    while len(parts) < expected_fields:
        parts.append('')

    # Validate fields
    result.add_info("Number", f"Value: '{parts[1]}'")
    validate_hex_timestamp(parts[2], "Time", result)
    result.add_info("Sender", f"Value: '{parts[3]}'")
    validate_classification(parts[4], result)
    result.add_info("Acknowledgement", f"Value: '{parts[5]}'")
    result.add_info("MAC", f"Value: '{parts[6]}'")
    result.add_info("Recipient", f"Value: '{parts[7]}'")
    validate_text_type(parts[8], result)
    validate_encoding(parts[9], result)

    # Text is mandatory, max 2048 bytes
    text = parts[10] if len(parts) > 10 else ''
    if not text:
        result.add_error("Text", "Missing (mandatory)")
    elif len(text) > 2048:
        result.add_error("Text", f"Length {len(text)} exceeds 2048 bytes")
    else:
        result.add_info("Text", f"Value: '{text[:100]}...' ({len(text)} chars)")

    return result


def generate_sample_contact_message():
    """Generate a sample CONTACT message using our plugin logic for testing."""
    import time
    import base64

    timestamp = format(int(time.time() * 1000), 'X')

    parts = [
        "CONTACT",
        "",                      # Number (empty per example)
        timestamp,               # Time
        "+4917663283066",        # Sender (phone number)
        "U",                     # Classification
        "",                      # Acknowledgement
        "",                      # MAC
        "INC-TEST123",           # ContactID
        "FALSE",                 # DeleteFlag
        "52.4601683",            # Latitude
        "13.3958967",            # Longitude
        "0",                     # Altitude
        "",                      # RelX
        "",                      # RelY
        "",                      # RelZ
        "",                      # Speed
        "",                      # Course
        "",                      # Heading
        "",                      # Roll
        "",                      # Pitch
        "",                      # Width
        "",                      # Length
        "",                      # Height
        "SIMS",                  # Name
        "M",                     # Source
        "SUGP-----------",       # SIDC (15 chars)
        "",                      # MMSI
        "",                      # ICAO
        "",                      # Image (would be BASE64)
        base64.b64encode(b"Test incident description").decode('ascii')  # Comment
    ]

    return ";".join(parts) + "\r\n"


def generate_sample_text_message():
    """Generate a sample TEXT message using our plugin logic for testing."""
    import time

    timestamp = format(int(time.time() * 1000), 'X')

    parts = [
        "TEXT",
        "",                      # Number
        timestamp,               # Time
        "SIMS",                  # Sender
        "U",                     # Classification
        "FALSE",                 # Acknowledgement
        "",                      # MAC
        "",                      # Recipient (broadcast)
        "1",                     # Type (1=Alert)
        "NONE",                  # Encoding
        '"New incident: Test Alert"'  # Text
    ]

    return ";".join(parts) + "\r\n"


def test_with_database():
    """Test SEDAP message generation with actual database incidents."""
    from dotenv import load_dotenv
    load_dotenv()

    database_url = os.getenv('DATABASE_URL')
    if not database_url:
        print("DATABASE_URL not set, skipping database test")
        return

    print("\n" + "=" * 70)
    print("--- Testing with Database Incidents ---")
    print("=" * 70)

    try:
        engine = create_engine(database_url)
        Session = sessionmaker(bind=engine)
        db = Session()

        # Import models
        from models.incident_model import IncidentORM
        from models.media_model import MediaORM

        # Get recent incidents
        incidents = db.query(IncidentORM).order_by(IncidentORM.created_at.desc()).limit(3).all()

        if not incidents:
            print("No incidents found in database")
            return

        for incident in incidents:
            print(f"\n--- Incident: {incident.incident_id} ---")

            # Get media for incident
            media_items = db.query(MediaORM).filter(MediaORM.incident_id == incident.id).all()
            image_url = None
            for m in media_items:
                if m.media_type == 'image':
                    image_url = m.file_url
                    break

            # Build incident dict like integration_delivery_service does
            incident_dict = {
                'id': str(incident.id),
                'incident_id': incident.incident_id,
                'title': incident.title,
                'description': incident.description,
                'priority': incident.priority,
                'status': incident.status,
                'category': incident.category,
                'latitude': incident.latitude,
                'longitude': incident.longitude,
                'heading': incident.heading,
                'user_phone': incident.user_phone,
                'image_url': image_url,
                'image_base64': ''  # Would be fetched by plugin
            }

            print(f"  Title: {incident.title}")
            print(f"  Description: {incident.description[:50] if incident.description else 'N/A'}...")
            print(f"  Location: {incident.latitude}, {incident.longitude}")
            print(f"  Reporter: {incident.user_phone}")
            print(f"  Image URL: {image_url or 'None'}")

            # Generate CONTACT message using plugin logic
            import time
            timestamp = format(int(time.time() * 1000), 'X')
            reporter = incident.user_phone or 'SIMS'
            comment_raw = incident.description or 'SIMS'
            comment = base64.b64encode(comment_raw.encode('utf-8')).decode('ascii')

            parts = [
                "CONTACT",
                "",                      # Number
                timestamp,               # Time
                reporter,                # Sender
                "U",                     # Classification
                "",                      # Acknowledgement
                "",                      # MAC
                incident.incident_id,    # ContactID
                "FALSE",                 # DeleteFlag
                str(incident.latitude or 0),   # Latitude
                str(incident.longitude or 0),  # Longitude
                "0",                     # Altitude
                "", "", "",              # Relative coords
                "", "",                  # Speed, Course
                str(incident.heading or ""),  # Heading
                "", "",                  # Roll, Pitch
                "", "", "",              # Width, Length, Height
                "SIMS",                  # Name
                "M",                     # Source
                "SUGP-----------",       # SIDC
                "", "",                  # MMSI, ICAO
                "",                      # Image (empty for test)
                comment                  # Comment
            ]

            contact_msg = ";".join(parts) + "\r\n"
            print(f"\n  Generated CONTACT Message:")
            print(f"  {contact_msg[:200]}...")

            result = validate_contact_message(contact_msg)
            print(f"\n  Validation:")
            for msg in result.errors:
                print(f"    {msg}")
            for msg in result.warnings:
                print(f"    {msg}")
            print(f"  Valid: {result.is_valid()}")

        db.close()

    except Exception as e:
        print(f"Database test failed: {e}")
        import traceback
        traceback.print_exc()


def main():
    print("=" * 70)
    print("SEDAP Message Validator - ICD v1.0 Compliance Check")
    print("=" * 70)

    # Generate and validate sample CONTACT message
    print("\n--- Sample CONTACT Message ---")
    contact_msg = generate_sample_contact_message()
    print(f"Message:\n{contact_msg}")

    result = validate_contact_message(contact_msg)
    print("\nValidation Results:")
    result.print_results()
    print(f"\nValid: {result.is_valid()}")

    # Generate and validate sample TEXT message
    print("\n" + "-" * 70)
    print("\n--- Sample TEXT Message ---")
    text_msg = generate_sample_text_message()
    print(f"Message:\n{text_msg}")

    result = validate_text_message(text_msg)
    print("\nValidation Results:")
    result.print_results()
    print(f"\nValid: {result.is_valid()}")

    # Test with the example from user
    print("\n" + "=" * 70)
    print("--- Validating User's Example CONTACT Message ---")
    user_example = "CONTACT;;019B11E9C21D;SIMS;U;;;F0F1;FALSE;52.4601683;13.3958967;0;;;;;;;;;;;;SIMS;M;SUGP-----------;;;;"
    print(f"Message:\n{user_example}")

    result = validate_contact_message(user_example)
    print("\nValidation Results:")
    result.print_results()
    print(f"\nValid: {result.is_valid()}")

    # Test with database
    test_with_database()

    print("\n" + "=" * 70)
    print("Validation complete.")


if __name__ == "__main__":
    main()
