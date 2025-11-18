#!/usr/bin/env python3
import struct
import subprocess
import argparse

HDR_FMT = "<Q I I Q Q Q Q Q"    # mp_file_header, little-endian
SITE_FMT = "<Q Q Q"             # mp_file_site

def symbolize(pc, binary):
    if not binary:
        return ""
    try:
        out = subprocess.check_output(
            ["addr2line", "-e", binary, hex(pc)],
            stderr=subprocess.DEVNULL,
        )
        return out.decode("utf-8").strip()
    except Exception:
        return ""

def read_profile(path, binary=None, top=20):
    with open(path, "rb") as f:
        hdr_raw = f.read(struct.calcsize(HDR_FMT))
        hdr = struct.unpack(HDR_FMT, hdr_raw)
        magic, version, _, stride, alloc_count, sample_count, overflow, n_sites = hdr
        estimated = sample_count * stride  # stride from header

        print(f"File: {path}")
        print(f"  stride_bytes  = {stride}")
        print(f"  alloc_count   = {alloc_count}")
        print(f"  sample_count  = {sample_count}")
        print(f"  site_overflow = {overflow}")
        print(f"  n_sites       = {n_sites}")


        sites = []
        for _ in range(n_sites):
            site_raw = f.read(struct.calcsize(SITE_FMT))
            pc, sample_cnt, total_bytes = struct.unpack(SITE_FMT, site_raw)
            sites.append((total_bytes, sample_cnt, pc))

        # Sort sites by total sampled bytes (descending)
        sites.sort(reverse=True)

        print(f"Top {min(top, len(sites))} sites by estimated total bytes:")

        for total_bytes, sample_cnt, pc in sites[:top]:
            # Estimated total allocated bytes at this site = sample_count * stride
            estimated = sample_cnt * stride
            loc = symbolize(pc, binary)
            print(f"  pc={hex(pc)} est_bytes={estimated} bytes={total_bytes} samples={sample_cnt}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("file", help="profile .bin file")
    ap.add_argument("-e", "--binary", help="path to executable for symbolization")
    ap.add_argument("--top", type=int, default=20)
    args = ap.parse_args()
    read_profile(args.file, binary=args.binary, top=args.top)

if __name__ == "__main__":
    main()