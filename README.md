# ECC-Based Address Binding for Memory Aliasing Detection

This project implements a lightweight defense against **DRAM aliasing attacks** by binding physical address information into ECC (Error-Correcting Code).

The design augments standard **SECDED (Single Error Correction, Double Error Detection)** ECC with an address-derived identifier, enabling detection of incorrect memory mappings at runtime.

---

## Overview

Memory aliasing attacks allow two physical addresses to map to the same DRAM location, breaking isolation in systems like TEEs.

This project introduces:
- Address-bound ECC using XOR-based binding
- SplitMix64-based block ID generation
- Detection via ECC inconsistencies on read
- Large-scale simulation across 2³⁰ address space

---

## File Structure

- Core implementation:
  - `test_utils.cpp` — ECC encode/decode, binding, mixing, helpers

- Test programs:
  - `tests1234.cpp` — ECC correctness tests
  - `test5.cpp` — One-bit-off address aliasing (sectioned)
  - `test6.cpp` — Two-bit-off address aliasing (sectioned)
  - `test7.cpp` — Random aliasing test (sectioned)

---

## Compilation

```bash
g++ -O3 -std=c++17 test_utils.cpp tests1234.cpp -o tests1234
g++ -O3 -std=c++17 test_utils.cpp test5.cpp -o test5
g++ -O3 -std=c++17 test_utils.cpp test6.cpp -o test6
g++ -O3 -std=c++17 test_utils.cpp test7.cpp -o test7
```
## Usage

For basic ECC validation:
```bash
./tests1234
```
One-bit off address aliasing (exhaustive):
```bash
./test5 <SECTION_ID> <NUM_SECTIONS> <BASE_SEED>
```
Two-bit off address aliasing (exhaustive):
```bash
./test6 <SECTION_ID> <NUM_SECTIONS> <BASE_SEED>
```
Random address aliasing test (exhaustive):
```bash
./test7 <SECTION_ID> <NUM_SECTIONS> <BASE_SEED>
```
Tests 5-7 have partition capability since they can be lengthy as they exhaustively test a 30 bit address space.

For the data included in the report, the following configurations were used for the sectioned tests:
```bash
./test5 <0-19> 20  111111111
./test6 <0-99> 100 123456789
./test7 <0-19> 20  987654321
```
## Output Metrics
ECC uncorrectable errors: uncorrectable error in block / number of trials

ECC correction threshold met: correction threshold exceeded in block / number of trials




