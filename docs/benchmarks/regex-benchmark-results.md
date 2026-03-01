# Regex Benchmark Results

Generated at: 2026-03-01T10:09:30.815593+00:00

## Environment

- Product: `macOS Tahoe (26.3)`
- Kernel: `darwin 25.3.0`
- CPU architecture: `x86_64`
- Qt: `6.10.1`
- Iterations: `5` measured + `1` warmup

## Regex Profiles

| Profile | Pattern |
| --- | --- |
| simple | `ERROR` |
| normal | `level=(ERROR|WARN).*component=(auth|scheduler|replication|storage|parser|crawler).*msg=\"(timeout|failed|exception).*\".*path=/api/v1/(task|job)/[0-9]{1,4}.*shard=[0-9]{1,2} retry=false` |
| complex | `level=(INFO|WARN) component=(auth|scheduler|replication|storage|parser|crawler).*req=[A-F0-9]{16}( session=([A-F0-9]{8}|[A-F0-9]{16}))?.*msg=\"(peer (reset|disconnect(ed)?) during remote link handover|(timeout|failed|exception) while processing customer ledger batch)\".*path=/api/v1/(node|task)/[0-9]{1,4}.*tenant=prod region=us-east-1 shard=[0-9]{1,2} (window=batch-e2e|retry=false)` |

## Median Search Time

### simple

| Engine | 50MB (ms) | 500MB (ms) | 5GB (ms) |
| --- | ---: | ---: | ---: |
| qt | 56.32 | 523.41 | 5358.14 |
| vectorscan-generic | 20.81 | 169.98 | 2019.36 |
| vectorscan-avx | 17.41 | 169.92 | 1890.23 |

### normal

| Engine | 50MB (ms) | 500MB (ms) | 5GB (ms) |
| --- | ---: | ---: | ---: |
| qt | 65.40 | 690.74 | 6448.53 |
| vectorscan-generic | 28.27 | 186.47 | 2165.40 |
| vectorscan-avx | 26.45 | 180.23 | 2051.90 |

### complex

| Engine | 50MB (ms) | 500MB (ms) | 5GB (ms) |
| --- | ---: | ---: | ---: |
| qt | 113.77 | 1071.93 | 11172.74 |
| vectorscan-generic | 38.02 | 219.07 | 2435.64 |
| vectorscan-avx | 36.15 | 204.00 | 2496.90 |

## Median Throughput

### simple

| Engine | 50MB (MiB/s) | 500MB (MiB/s) | 5GB (MiB/s) |
| --- | ---: | ---: | ---: |
| qt | 887.84 | 955.42 | 955.70 |
| vectorscan-generic | 2403.28 | 2941.92 | 2535.84 |
| vectorscan-avx | 2872.35 | 2943.08 | 2709.07 |

### normal

| Engine | 50MB (MiB/s) | 500MB (MiB/s) | 5GB (MiB/s) |
| --- | ---: | ---: | ---: |
| qt | 764.64 | 723.97 | 794.10 |
| vectorscan-generic | 1768.70 | 2681.74 | 2364.81 |
| vectorscan-avx | 1890.98 | 2774.69 | 2495.62 |

### complex

| Engine | 50MB (MiB/s) | 500MB (MiB/s) | 5GB (MiB/s) |
| --- | ---: | ---: | ---: |
| qt | 439.54 | 466.52 | 458.33 |
| vectorscan-generic | 1315.13 | 2282.76 | 2102.43 |
| vectorscan-avx | 1383.41 | 2451.30 | 2050.85 |

## 500MB Snapshot

| Profile | Qt (ms) | Vectorscan generic (ms) | Vectorscan AVX (ms) | AVX vs generic |
| --- | ---: | ---: | ---: | ---: |
| simple | 523.41 | 169.98 | 169.92 | +0.04% |
| normal | 690.74 | 186.47 | 180.23 | +3.35% |
| complex | 1071.93 | 219.07 | 204.00 | +6.88% |

## Search Coverage And Hit Rate

| Profile | Size | Engine | Searched lines | Matches | Hit rate |
| --- | --- | --- | ---: | ---: | ---: |
| simple | 50MB | qt | 227671 | 19870 | 8.73% |
| simple | 50MB | vectorscan-generic | 227671 | 19870 | 8.73% |
| simple | 50MB | vectorscan-avx | 227671 | 19870 | 8.73% |
| simple | 500MB | qt | 2276685 | 198692 | 8.73% |
| simple | 500MB | vectorscan-generic | 2276685 | 198692 | 8.73% |
| simple | 500MB | vectorscan-avx | 2276685 | 198692 | 8.73% |
| simple | 5GB | qt | 23313282 | 2034614 | 8.73% |
| simple | 5GB | vectorscan-generic | 23313282 | 2034614 | 8.73% |
| simple | 5GB | vectorscan-avx | 23313282 | 2034614 | 8.73% |
| normal | 50MB | qt | 227671 | 6831 | 3.00% |
| normal | 50MB | vectorscan-generic | 227671 | 6831 | 3.00% |
| normal | 50MB | vectorscan-avx | 227671 | 6831 | 3.00% |
| normal | 500MB | qt | 2276685 | 68301 | 3.00% |
| normal | 500MB | vectorscan-generic | 2276685 | 68301 | 3.00% |
| normal | 500MB | vectorscan-avx | 2276685 | 68301 | 3.00% |
| normal | 5GB | qt | 23313282 | 699399 | 3.00% |
| normal | 5GB | vectorscan-generic | 23313282 | 699399 | 3.00% |
| normal | 5GB | vectorscan-avx | 23313282 | 699399 | 3.00% |
| complex | 50MB | qt | 227671 | 9108 | 4.00% |
| complex | 50MB | vectorscan-generic | 227671 | 9108 | 4.00% |
| complex | 50MB | vectorscan-avx | 227671 | 9108 | 4.00% |
| complex | 500MB | qt | 2276685 | 91068 | 4.00% |
| complex | 500MB | vectorscan-generic | 2276685 | 91068 | 4.00% |
| complex | 500MB | vectorscan-avx | 2276685 | 91068 | 4.00% |
| complex | 5GB | qt | 23313282 | 932532 | 4.00% |
| complex | 5GB | vectorscan-generic | 23313282 | 932532 | 4.00% |
| complex | 5GB | vectorscan-avx | 23313282 | 932532 | 4.00% |

