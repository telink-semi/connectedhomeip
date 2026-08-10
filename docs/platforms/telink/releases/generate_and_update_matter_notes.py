#!/usr/bin/env python3
#
#    Copyright (c) 2026 Telink Semiconductor Co., Ltd.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

"""
Telink Matter SDK - Generate and Release Notes Update Script

This script:
  1. Builds all Telink Matter examples for supported boards using build_examples.py
  2. Captures build logs and extracts memory usage information (RAM/ROM)
  3. Updates the Release Note working copy
     (docs/platforms/telink/releases/telink_release_notes.md) in-place,
     using table format for the Resource Usage section
  4. Collects firmware files (zephyr.bin/.elf/.hex, .config, zephyr.dts) and
     creates per-board zip archives

Usage (run from the connectedhomeip repo root):
    python3 docs/platforms/telink/releases/generate_and_update_matter_notes.py                  # full run
    python3 docs/platforms/telink/releases/generate_and_update_matter_notes.py --skip-build     # skip building
    python3 docs/platforms/telink/releases/generate_and_update_matter_notes.py --skip-firmware  # skip firmware packaging
"""

import re
import shutil
import subprocess
from pathlib import Path
from typing import Dict, List, Optional, Tuple


class TelinkMatterBuildManager:
    """Manages building Telink Matter examples and generating release notes."""

    def __init__(self, repo_root: Optional[Path] = None):
        # This script lives in docs/platforms/telink/releases/. Detect the repo
        # root by walking up from the script's location looking for the `.git`
        # marker, so the script works regardless of where it is invoked from.
        if repo_root is not None:
            self.repo_root = repo_root
        else:
            candidate = Path(__file__).resolve().parent
            while candidate != candidate.parent:
                if (candidate / ".git").exists():
                    break
                candidate = candidate.parent
            self.repo_root = candidate

        # Output directories
        self.release_dir = self.repo_root / "build_for_release"
        self.release_dir.mkdir(exist_ok=True)

        self.build_logs_dir = self.release_dir / "build_logs"
        self.build_logs_dir.mkdir(exist_ok=True)

        self.firmware_output_dir = self.release_dir / "firmware"
        self.firmware_output_dir.mkdir(exist_ok=True)

        # Release Note working copy (read and write in-place; only the
        # Resource Usage section is regenerated, the rest is hand-maintained).
        # The working copy lives next to this script in
        # docs/platforms/telink/releases/. A versioned snapshot
        # (telink_release_notes_<tag>.md) is produced by
        # scripts/tools/telink/package_telink_firmware.sh at release time.
        self.output_path = Path(__file__).resolve().parent / "telink_release_notes.md"

        # Board family mapping (board -> chip family)
        self.board_family = {
            "tlsr9518adk80d": "TLSR951X",
            "tlsr9528a": "TLSR952X",
            "tlsr9528a_retention": "TLSR952X",
            "tlsr9118bdk40d": "TLSR911X",
            "tl3218x": "TL321X",
            "tl3218x_ml3m": "TL321X",
            "tl3218x_retention": "TL321X",
            "tl3238x": "TL323X",
            "tl3238x_retention": "TL323X",
            "tl7218x": "TL721X",
            "tl7218x_retention": "TL721X",
        }

        # Display order for boards in the release note
        self.board_order = [
            ("tlsr9518adk80d", "TLSR951X/B91"),
            ("tlsr9528a", "TLSR952X/B92"),
            ("tl3218x", "TL321X"),
            ("tl3238x", "TL323X"),
            ("tl7218x", "TL721X"),
            ("tlsr9118bdk40d", "TLSR911X/W91"),
        ]

        # Define all build targets (board, app, target_string)
        # target_string is the full build_examples.py target (without the 'telink-' prefix)
        self.build_targets = self._get_build_targets()

    def _get_build_targets(self) -> List[Tuple[str, str, str]]:
        """Define all Matter build targets for Telink platforms.

        Returns a list of (board, app_name, target_suffix) tuples where:
          - board: the board identifier (without 'telink-' prefix)
          - app_name: human-readable app name for display
          - target_suffix: the full target string passed to build_examples.py
                           (e.g. 'tl3238x-light-ota-factory-data-4mb')
        """
        targets = []

        # -------------------------- TL323X --------------------------
        targets.extend([
            ("tl3238x", "lighting-app", "tl3238x-light-ota-factory-data-4mb"),
            ("tl3238x_retention", "light-switch-app", "tl3238x_retention-light-switch-ota-compress-lzma-factory-data"),
        ])

        # -------------------------- TL721X --------------------------
        targets.extend([
            ("tl7218x", "lighting-app", "tl7218x-light-ota-compress-lzma-shell-factory-data"),
            ("tl7218x", "bridge-app", "tl7218x-bridge"),
            ("tl7218x", "window-app", "tl7218x-window-covering"),
            ("tl7218x_retention", "light-switch-app", "tl7218x_retention-light-switch-ota-compress-lzma-factory-data"),
        ])

        # -------------------------- TLSR952X/B92 --------------------------
        targets.extend([
            ("tlsr9528a", "air-quality-sensor-app", "tlsr9528a_retention-air-quality-sensor"),
            ("tlsr9528a", "all-clusters-minimal-app", "tlsr9528a-all-clusters-minimal-nfc-payload"),
            ("tlsr9528a", "contact-sensor-app", "tlsr9528a_retention-contact-sensor"),
            ("tlsr9528a", "light-switch-app", "tlsr9528a-light-switch-ota-compress-lzma-shell-factory-data"),
            ("tlsr9528a", "lock-app", "tlsr9528a-lock-dfu-smp"),
            ("tlsr9528a", "smoke-co-alarm-app", "tlsr9528a_retention-smoke-co-alarm"),
        ])

        # -------------------------- TLSR911X/W91 --------------------------
        targets.extend([
            ("tlsr9118bdk40d", "lighting-app", "tlsr9118bdk40d-light-ota-factory-data-log-progress"),
            ("tlsr9118bdk40d", "all-clusters-app", "tlsr9118bdk40d-all-clusters"),
            ("tlsr9118bdk40d", "thermostat", "tlsr9118bdk40d-thermostat"),
        ])

        # -------------------------- TLSR951X/B91 --------------------------
        targets.extend([
            ("tlsr9518adk80d", "lighting-app", "tlsr9518adk80d-light-ota-rpc-factory-data-4mb"),
            ("tlsr9518adk80d", "pump-controller-app", "tlsr9518adk80d-pump-controller"),
            ("tlsr9518adk80d", "shell", "tlsr9518adk80d-shell"),
        ])

        # -------------------------- TL321X --------------------------
        targets.extend([
            ("tl3218x", "lighting-app", "tl3218x-light-ota-compress-lzma-shell-factory-data"),
            ("tl3218x", "ota-requestor-app", "tl3218x-ota-requestor"),
            ("tl3218x_retention", "light-switch-app", "tl3218x_retention-light-switch-ota-factory-data"),
        ])

        return targets

    def build_all(self):
        """Build all Telink Matter examples using build_examples.py."""
        print("=" * 80)
        print("Building all Telink Matter examples...")
        print("=" * 80)
        print()

        success_count = 0
        fail_count = 0

        for board, app_name, target_suffix in self.build_targets:
            full_target = f"telink-{target_suffix}"
            # build_examples.py outputs to out/<full_target>
            log_filename = f"build_{target_suffix}.log"
            log_file = self.build_logs_dir / log_filename

            print(f"  Building {full_target}...")
            print(f"  App: {app_name}  Board: {board}")

            # The build is run via scripts/build/build_examples.py
            cmd = [
                "./scripts/build/build_examples.py",
                "--target", full_target,
                "build",
            ]

            print(f"  Command: {' '.join(cmd)}")

            # Run inside the build environment wrapper if available
            run_in_build_env = self.repo_root / "scripts" / "run_in_build_env.sh"
            if run_in_build_env.exists():
                cmd = [str(run_in_build_env), " ".join(cmd)]

            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                cwd=str(self.repo_root),
                check=False,
            )

            # Write log file
            with open(log_file, "w", encoding="utf-8") as f:
                f.write(f"Target: {full_target}\n")
                f.write(f"Board: {board}\n")
                f.write(f"App: {app_name}\n\n")
                f.write(f"Command: {' '.join(cmd)}\n\n")
                f.write(f"STDOUT:\n{result.stdout}\n\n")
                f.write(f"STDERR:\n{result.stderr}\n\n")
                f.write(f"Return code: {result.returncode}\n")

            if result.returncode == 0:
                print(f"  Success! Log written to {log_file}")
                success_count += 1
            else:
                print(f"  Failed! Log written to {log_file}")
                fail_count += 1

            print()

        print("=" * 80)
        print(f"Build complete! {success_count} succeeded, {fail_count} failed.")
        print("=" * 80)

    def extract_memory_info(self) -> Dict[str, Dict[str, List[Dict]]]:
        """Extract memory usage information from build logs.

        Returns a nested dict: {family: {board: [{name, memory: [{name,used,size,percent}]}}]}
        """
        data: Dict[str, Dict[str, List[Dict]]] = {}

        print()
        print("=" * 80)
        print("Extracting memory usage information from logs...")
        print("=" * 80)
        print()

        for log_file in self.build_logs_dir.glob("build_*.log"):
            filename = log_file.name
            # filename: build_<target_suffix>.log
            target_suffix = filename[len("build_"):-len(".log")]

            # Identify the board from the target suffix
            board = None
            app_name = None
            for b in sorted(self.board_family.keys(), key=lambda x: -len(x)):
                if target_suffix.startswith(b):
                    board = b
                    rest = target_suffix[len(b):]
                    if rest.startswith("-"):
                        rest = rest[1:]
                    # The app name is the segment up to the next '-<option>'
                    # Known app prefixes
                    app_prefixes = [
                        "light", "light-switch", "bridge", "window-covering",
                        "air-quality-sensor", "all-clusters-minimal", "all-clusters",
                        "contact-sensor", "lock", "smoke-co-alarm",
                        "pump", "pump-controller", "shell",
                        "temperature-measurement", "thermostat", "ota-requestor",
                    ]
                    for ap in app_prefixes:
                        if rest.startswith(ap):
                            app_name = ap
                            break
                    break

            if not board:
                continue

            if not app_name:
                app_name = target_suffix

            # Read the log content
            with open(log_file, "r", encoding="utf-8", errors="ignore") as f:
                content = f.read()

            # Find the "Memory region" section (Zephyr output)
            idx = content.find("Memory region")
            if idx == -1:
                continue

            # Extract memory region text
            memory_end = content.find("Generating files from", idx)
            if memory_end == -1:
                memory_end = content.find("Loaded", idx)
            if memory_end == -1:
                memory_end = idx + 800

            memory_text = content[idx:memory_end]

            # Parse memory regions
            memory_regions = []
            lines = memory_text.split("\n")
            for line in lines[1:]:
                line = line.strip()
                if not line:
                    continue
                if "IDT_LIST" in line:
                    break

                # Format: RAMILM:         21248 B      131072 B      16.21%
                match = re.match(r"(\S+):\s*(\d+)\s*(\S+)\s*(\d+)\s*(\S+)\s*([\d.]+)%", line)
                if match:
                    name = match.group(1).rstrip(":")
                    used = f"{match.group(2)} {match.group(3)}"
                    size = f"{match.group(4)} {match.group(5)}"
                    percent = f"{match.group(6)}%"
                    memory_regions.append({
                        "name": name,
                        "used": used,
                        "size": size,
                        "percent": percent,
                    })
                else:
                    # Format: RAM_ILM_N:         27 KB       128 KB     21.09%
                    match2 = re.match(r"(\S+):\s*([\d.]+)\s*(\S+)\s*([\d.]+)\s*(\S+)\s*([\d.]+)%", line)
                    if match2:
                        name = match2.group(1).rstrip(":")
                        used = f"{match2.group(2)} {match2.group(3)}"
                        size = f"{match2.group(4)} {match2.group(5)}"
                        percent = f"{match2.group(6)}%"
                        memory_regions.append({
                            "name": name,
                            "used": used,
                            "size": size,
                            "percent": percent,
                        })

            if memory_regions:
                # Map board to base board and family
                base_board = board
                # Strip retention/ml3m/ml7g/ml7m suffixes for grouping
                for suffix in ["_retention", "_ml3m"]:
                    if base_board.endswith(suffix):
                        base_board = base_board[: -len(suffix)]
                        break

                family = self.board_family.get(board, base_board.upper())

                if family not in data:
                    data[family] = {}
                if base_board not in data[family]:
                    data[family][base_board] = []

                # Build a display name for the sample (target suffix)
                display_name = target_suffix

                data[family][base_board].append({
                    "name": display_name,
                    "app": app_name,
                    "memory": memory_regions,
                })

        total_samples = sum(len(b) for f in data.values() for b in f.values())
        print(f"Processed {total_samples} samples successfully!")

        return data

    def update_release_notes(self, data: Dict[str, Dict[str, List[Dict]]]):
        """Update the Telink release note working copy in-place."""
        print()
        print("=" * 80)
        print(f"Updating {self.output_path.relative_to(self.repo_root)}...")
        print("=" * 80)
        print()

        # Check that the release note file exists
        if not self.output_path.exists():
            print(f"Error: Release note file {self.output_path} does not exist!")
            return

        # Build the Resource Usage section (table format)
        resource_usage_lines = []
        resource_usage_lines.append("## 📊 Resource Usage (Code Size)")
        resource_usage_lines.append("")
        resource_usage_lines.append(
            "This section shows the RAM and ROM usage for various Matter examples "
            "on Telink platforms, built with the Matter SDK and Zephyr RTOS."
        )
        resource_usage_lines.append("")
        resource_usage_lines.append("### Supported Boards")
        resource_usage_lines.append("")
        resource_usage_lines.append("| Board | Chip Family |")
        resource_usage_lines.append("|-------|-------------|")
        for board_name, family_name in self.board_order:
            resource_usage_lines.append(f"| {board_name} | {family_name} |")
        resource_usage_lines.append("")
        resource_usage_lines.append("---")
        resource_usage_lines.append("")

        for board_name, family_name in self.board_order:
            # Find matching family in data
            family_key = None
            for fk in data:
                if fk.startswith(family_name.split("/")[0]) or family_name.startswith(fk):
                    family_key = fk
                    break
            if not family_key or board_name not in data.get(family_key, {}):
                print(f"Warning: No data for {board_name} ({family_name})")
                continue

            resource_usage_lines.append(f"### {family_name} ({board_name})")
            resource_usage_lines.append("")
            resource_usage_lines.append("📈 **Resource Usage Details**")
            resource_usage_lines.append("")

            samples = sorted(data[family_key][board_name], key=lambda x: x["name"])

            if not samples:
                continue

            # Use region names from the first sample as table headers
            region_names = [r["name"] for r in samples[0]["memory"]]

            # Table header
            headers = ["Sample"] + region_names
            resource_usage_lines.append("| " + " | ".join(headers) + " |")
            resource_usage_lines.append("|" + "|".join(["---" for _ in headers]) + "|")

            # Data rows
            for sample in samples:
                row = [f"**{sample['name']}**"]
                region_map = {r["name"]: r for r in sample["memory"]}
                for region_name in region_names:
                    if region_name in region_map:
                        r = region_map[region_name]
                        row.append(f"{r['used']} ({r['percent']} of {r['size']})")
                    else:
                        row.append("N/A")
                resource_usage_lines.append("| " + " | ".join(row) + " |")

            resource_usage_lines.append("")
            resource_usage_lines.append("---")
            resource_usage_lines.append("")

        # Read the release note and replace the Resource Usage section
        with open(self.output_path, "r", encoding="utf-8") as f:
            template_lines = f.read().split("\n")

        new_lines = []
        i = 0
        while i < len(template_lines):
            line = template_lines[i]

            # Match the Resource Usage section start
            if "Resource Usage" in line and line.startswith("#"):
                # Add the generated Resource Usage content
                new_lines.extend(resource_usage_lines)

                # Skip the original section until "Additional Notes"
                i += 1
                while i < len(template_lines):
                    if "Additional Notes" in template_lines[i] and template_lines[i].startswith("#"):
                        break
                    i += 1
                continue

            new_lines.append(line)
            i += 1

        # Write the final release note
        with open(self.output_path, "w", encoding="utf-8") as f:
            f.write("\n".join(new_lines))

        print(f"Updated {self.output_path.relative_to(self.repo_root)} successfully!")

    def collect_firmware(self):
        """Collect firmware files from build output directories."""
        print()
        print("=" * 80)
        print("Collecting firmware files...")
        print("=" * 80)
        print()

        firmware_files = [
            "zephyr.bin",
            "zephyr.elf",
            "zephyr.hex",
            "zephyr.dts",
            ".config",
            "zephyr.signed.bin",
            "mcuboot.bin",
        ]

        board_firmware: Dict[str, List[Dict]] = {}

        for board, app_name, target_suffix in self.build_targets:
            full_target = f"telink-{target_suffix}"
            # build_examples.py outputs to out/<full_target>/zephyr/
            build_output_dir = self.repo_root / "out" / full_target / "zephyr"

            if not build_output_dir.exists():
                print(f"  Skipping {full_target}: output dir not found")
                continue

            # Base board name (strip retention/ml3m/ml7g/ml7m suffixes)
            base_board_name = board
            for suffix in ["_retention", "_ml3m", "_ml7g", "_ml7m"]:
                if base_board_name.endswith(suffix):
                    base_board_name = base_board_name[: -len(suffix)]
                    break

            if base_board_name not in board_firmware:
                board_firmware[base_board_name] = []

            # Create firmware output directory
            sample_firmware_dir = self.firmware_output_dir / base_board_name / target_suffix
            sample_firmware_dir.mkdir(parents=True, exist_ok=True)

            # Copy firmware files
            copied_files = []
            for fw_file in firmware_files:
                src_file = build_output_dir / fw_file
                if src_file.exists():
                    dst_file = sample_firmware_dir / fw_file
                    shutil.copy2(src_file, dst_file)
                    copied_files.append(fw_file)

            if copied_files:
                board_firmware[base_board_name].append({
                    "sample_name": target_suffix,
                    "directory": sample_firmware_dir,
                    "files": copied_files,
                })
                print(f"  Collected: {base_board_name}/{target_suffix} - {len(copied_files)} files")

        return board_firmware

    def create_firmware_archives(self, board_firmware: Dict[str, List[Dict]]):
        """Create per-board firmware zip archives."""
        print()
        print("=" * 80)
        print("Creating firmware archives...")
        print("=" * 80)
        print()

        archives_created = []

        for board_name, samples in board_firmware.items():
            if not samples:
                continue

            archive_path = self.firmware_output_dir / f"{board_name}_matter_firmware.zip"

            if archive_path.exists():
                archive_path.unlink()

            print(f"  Creating archive for {board_name}...")

            import zipfile
            with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED) as zipf:
                for sample in samples:
                    sample_dir = sample["directory"]
                    for file_name in sample["files"]:
                        file_path = sample_dir / file_name
                        arcname = f"{board_name}/{sample['sample_name']}/{file_name}"
                        zipf.write(file_path, arcname)

            archives_created.append(archive_path)
            print(f"  Created: {archive_path}")

        return archives_created

    def run(self, build: bool = True, collect_firmware: bool = True):
        """Run the full build + release note + firmware packaging flow."""
        if build:
            self.build_all()

        data = self.extract_memory_info()
        self.update_release_notes(data)

        if collect_firmware:
            board_firmware = self.collect_firmware()
            self.create_firmware_archives(board_firmware)

        print()
        print("=" * 80)
        print("All tasks complete!")
        print("=" * 80)


def main():
    import argparse

    parser = argparse.ArgumentParser(
        description="Build Telink Matter examples and update release notes."
    )
    parser.add_argument(
        "--skip-build", action="store_true",
        help="Skip building examples, just update release notes from existing logs"
    )
    parser.add_argument(
        "--skip-firmware", action="store_true",
        help="Skip collecting and archiving firmware"
    )
    args = parser.parse_args()

    manager = TelinkMatterBuildManager()
    manager.run(build=not args.skip_build, collect_firmware=not args.skip_firmware)


if __name__ == "__main__":
    main()
