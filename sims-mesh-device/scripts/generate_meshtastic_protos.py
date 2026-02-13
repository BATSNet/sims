"""
PlatformIO pre-build script to generate nanopb C code from Meshtastic protobuf files.
This script runs before compilation to ensure protobuf encoders/decoders are available.
"""

import os
import sys
import subprocess
from pathlib import Path

Import("env")

def generate_protos():
    """Generate nanopb C code from all Meshtastic .proto files"""

    # Get project paths
    project_dir = Path(env.get("PROJECT_DIR"))
    proto_base_dir = project_dir / "proto"
    meshtastic_proto_dir = proto_base_dir / "meshtastic"
    google_proto_dir = proto_base_dir / "google" / "protobuf"
    output_dir = project_dir / "src" / "generated"

    # Find nanopb generator in libdeps
    platform_dir = env.PioPlatform()
    libdeps_dir = project_dir / ".pio" / "libdeps" / env["PIOENV"]
    nanopb_dir = libdeps_dir / "Nanopb"
    generator_script = nanopb_dir / "generator" / "nanopb_generator.py"
    protoc_binary = nanopb_dir / "generator" / "protoc"

    # Verify paths exist
    if not meshtastic_proto_dir.exists():
        print(f"[Protobuf] Meshtastic proto directory not found: {meshtastic_proto_dir}")
        return

    if not generator_script.exists():
        print(f"[Protobuf] Nanopb generator not found: {generator_script}")
        print("[Protobuf] Run 'pio lib install' first to install Nanopb")
        return

    if not protoc_binary.exists():
        print(f"[Protobuf] Protoc binary not found: {protoc_binary}")
        return

    # Create output directory
    output_dir.mkdir(parents=True, exist_ok=True)

    # Find all .proto files from both directories
    meshtastic_protos = sorted(meshtastic_proto_dir.glob("*.proto"))
    google_protos = []
    if google_proto_dir.exists():
        google_protos = sorted(google_proto_dir.glob("*.proto"))

    proto_files = google_protos + meshtastic_protos

    if not proto_files:
        print(f"[Protobuf] No .proto files found")
        return

    print(f"[Protobuf] Found {len(proto_files)} proto files ({len(google_protos)} google, {len(meshtastic_protos)} meshtastic)")

    print(f"[Protobuf] Generating nanopb C code to {output_dir}")

    # Generate code for each proto file
    success_count = 0
    for proto_file in proto_files:
        proto_name = proto_file.name

        try:
            # Build command: python nanopb_generator.py -I proto_base_dir -D output_dir proto_file
            # Use proto_base_dir so imports like "meshtastic/channel.proto" work correctly
            cmd = [
                sys.executable,
                str(generator_script),
                f"-I{proto_base_dir}",
                f"-D{output_dir}",
                str(proto_file)
            ]

            result = subprocess.run(
                cmd,
                cwd=project_dir,
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode == 0:
                print(f"[Protobuf]   OK {proto_name}")
                success_count += 1
            else:
                print(f"[Protobuf]   FAILED {proto_name}")
                if result.stderr:
                    print(f"[Protobuf]     Error: {result.stderr}")
                if result.stdout:
                    print(f"[Protobuf]     Output: {result.stdout}")

        except subprocess.TimeoutExpired:
            print(f"[Protobuf]   TIMEOUT {proto_name}")
        except Exception as e:
            print(f"[Protobuf]   ERROR {proto_name}: {e}")

    print(f"[Protobuf] Generation complete: {success_count}/{len(proto_files)} files")

    # Post-process nanopb.pb.h to fix missing include
    nanopb_header = output_dir / "meshtastic" / "nanopb.pb.h"
    if nanopb_header.exists():
        try:
            content = nanopb_header.read_text()
            if "google/protobuf/descriptor.pb.h" not in content:
                fixed_content = content.replace(
                    "#include <pb.h>",
                    "#include <pb.h>\n#include \"google/protobuf/descriptor.pb.h\""
                )
                nanopb_header.write_text(fixed_content)
                print("[Protobuf] Fixed nanopb.pb.h include directive")
        except Exception as e:
            print(f"[Protobuf] Warning: Could not fix nanopb.pb.h: {e}")

    # Verify output files were created (check both output_dir and nested dirs)
    generated_files = list(output_dir.glob("*.pb.*")) + list(output_dir.glob("*/*.pb.*")) + list(output_dir.glob("*/*/*.pb.*"))
    if generated_files:
        print(f"[Protobuf] Generated {len(generated_files)} output files")
    else:
        print("[Protobuf] WARNING: No output files generated!")

# Run the generator
try:
    generate_protos()
except Exception as e:
    print(f"[Protobuf] ERROR: {e}")
    import traceback
    traceback.print_exc()
