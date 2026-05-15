from __future__ import annotations

import struct
from pathlib import Path


def read_image_size(path: str | Path) -> tuple[int, int]:
    image_path = Path(path)
    suffix = image_path.suffix.lower()
    if suffix == ".png":
        return _read_png_size(image_path)
    if suffix in {".jpg", ".jpeg"}:
        return _read_jpeg_size(image_path)
    raise ValueError(f"Unsupported image format for size probe: {image_path.suffix}")


def _read_png_size(path: Path) -> tuple[int, int]:
    with path.open("rb") as handle:
        signature = handle.read(8)
        if signature != b"\x89PNG\r\n\x1a\n":
            raise ValueError(f"Invalid PNG signature: {path}")
        chunk_header = handle.read(8)
        if len(chunk_header) != 8:
            raise ValueError(f"Truncated PNG header: {path}")
        _length, chunk_type = struct.unpack(">I4s", chunk_header)
        if chunk_type != b"IHDR":
            raise ValueError(f"PNG missing IHDR chunk: {path}")
        ihdr = handle.read(8)
        if len(ihdr) != 8:
            raise ValueError(f"Truncated PNG IHDR payload: {path}")
        return struct.unpack(">II", ihdr)


def _read_jpeg_size(path: Path) -> tuple[int, int]:
    with path.open("rb") as handle:
        if handle.read(2) != b"\xff\xd8":
            raise ValueError(f"Invalid JPEG signature: {path}")

        while True:
            marker_prefix = handle.read(1)
            if not marker_prefix:
                break
            if marker_prefix != b"\xff":
                continue

            marker = handle.read(1)
            while marker == b"\xff":
                marker = handle.read(1)

            if not marker or marker in {b"\xd8", b"\xd9"}:
                continue

            segment_length_data = handle.read(2)
            if len(segment_length_data) != 2:
                break
            segment_length = struct.unpack(">H", segment_length_data)[0]
            if segment_length < 2:
                raise ValueError(f"Invalid JPEG segment length: {path}")

            marker_value = marker[0]
            if 0xC0 <= marker_value <= 0xC3 or 0xC5 <= marker_value <= 0xC7 or 0xC9 <= marker_value <= 0xCB or 0xCD <= marker_value <= 0xCF:
                frame_data = handle.read(5)
                if len(frame_data) != 5:
                    raise ValueError(f"Truncated JPEG frame header: {path}")
                _precision, height, width = struct.unpack(">BHH", frame_data)
                return width, height

            handle.seek(segment_length - 2, 1)

    raise ValueError(f"Could not locate JPEG frame size: {path}")
