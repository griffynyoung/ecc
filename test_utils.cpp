#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <cassert>
#include <chrono>
#include <iomanip>

using std::array;
using std::uint8_t;
using std::uint32_t;
using std::uint64_t;

static constexpr uint64_t MASK64 = 0xFFFFFFFFFFFFFFFFULL;

// -----------------------------
// Data structures
// -----------------------------
struct EncodedWord {
    uint64_t data; // 64 data bits
    uint8_t ecc;   // bit 0 = overall parity, bits 1..7 = Hamming parity bits for positions 1,2,4,8,16,32,64
};

struct DecodeResult {
    uint64_t data;
    std::string status; // "clean", "corrected"
};

struct ReadResult {
    array<uint64_t, 8> decoded_words{};
    array<std::string, 8> statuses{};
    bool ecc_error = false;
};

// -----------------------------
// Bit helpers
// -----------------------------
static inline int parity64(uint64_t x) {
#if defined(__GNUG__) || defined(__clang__)
    return __builtin_parityll(x);
#else
    x ^= x >> 32;
    x ^= x >> 16;
    x ^= x >> 8;
    x ^= x >> 4;
    x &= 0xFULL;
    return (0x6996 >> x) & 1;
#endif
}

static inline int popcount64(uint64_t x) {
#if defined(__GNUG__) || defined(__clang__)
    return __builtin_popcountll(x);
#else
    int c = 0;
    while (x) {
        x &= (x - 1);
        ++c;
    }
    return c;
#endif
}

// Map data bit index [0..63] to encoded Hamming position in [3..71], skipping powers of two
static constexpr array<int, 64> make_data_positions() {
    array<int, 64> pos{};
    int data_idx = 0;
    for (int enc_pos = 3; enc_pos < 72; ++enc_pos) {
        bool is_power_of_two = (enc_pos & (enc_pos - 1)) == 0;
        if (!is_power_of_two) {
            pos[data_idx++] = enc_pos;
            if (data_idx == 64) break;
        }
    }
    return pos;
}

static constexpr array<int, 64> DATA_POS = make_data_positions();

// Unmap  encoded position to data bit index, or -1 if parity or unused.
static constexpr array<int, 72> make_encoded_to_data() {
    array<int, 72> rev{};
    for (int i = 0; i < 72; ++i) rev[i] = -1;
    for (int i = 0; i < 64; ++i) rev[DATA_POS[i]] = i;
    return rev;
}

static constexpr array<int, 72> ENCODED_TO_DATA = make_encoded_to_data();

// -----------------------------
// SECDED encode/decode for one 64-bit word
// -----------------------------
static inline uint8_t encode_ecc(uint64_t data) {
    // ecc bit 0: overall parity
    // ecc bit 1..7: parity positions 1,2,4,8,16,32,64 respectively
    uint8_t ecc = 0;

    // Compute 7 Hamming parity bits
    for (int p_idx = 0; p_idx < 7; ++p_idx) {
        int parity_pos = 1 << p_idx;
        int p = 0;

        for (int bit = 0; bit < 64; ++bit) {
            if (DATA_POS[bit] & parity_pos) {
                p ^= int((data >> bit) & 1ULL);
            }
        }

        ecc |= uint8_t(p << (p_idx + 1));
    }

    // Overall parity is parity of data bits + hamming parity bits
    int overall = parity64(data) ^ parity64(uint64_t(ecc >> 1));
    ecc |= uint8_t(overall);

    return ecc;
}

static inline EncodedWord encode_word(uint64_t data) {
    return {data, encode_ecc(data)};
}

static inline DecodeResult decode_word(const EncodedWord& w_in) {
    uint64_t data = w_in.data;
    uint8_t ecc = w_in.ecc;

    int overall_actual = ecc & 1U;
    int overall_expected = parity64(data) ^ parity64(uint64_t(ecc >> 1));
    bool overall_correct = (overall_actual == overall_expected);

    int syndrome = 0;

    for (int p_idx = 0; p_idx < 7; ++p_idx) {
        int parity_pos = 1 << p_idx;
        int expected = 0;
        for (int bit = 0; bit < 64; ++bit) {
            if (DATA_POS[bit] & parity_pos) {
                expected ^= int((data >> bit) & 1ULL);
            }
        }
        int actual = (ecc >> (p_idx + 1)) & 1U;
        if (expected != actual) {
            syndrome += parity_pos;
        }
    }

    std::string status = "clean";

    // Case 1: overall parity bit only flipped
    if (syndrome == 0 && !overall_correct) {
        status = "corrected";
        return {data, status};
    }

    // Case 2: double-bit error
    if (syndrome != 0 && overall_correct) {
        throw std::runtime_error("Two errors detected.");
    }

    // Case 3: single-bit error somewhere else
    if (syndrome != 0 && !overall_correct) {
        if (syndrome > 0 && syndrome < 72) {
            int data_bit = ENCODED_TO_DATA[syndrome];
            if (data_bit >= 0) {
                data ^= (1ULL << data_bit);
            }
            status = "corrected";
            return {data, status};
        }
        throw std::runtime_error("Invalid syndrome.");
    }

    // Case 4: clean
    return {data, status};
}

// -----------------------------
// Block-level parity packing / binding
// -----------------------------
static inline uint64_t extract_parity_bits(const array<EncodedWord, 8>& words) {
    // each word contributes 8 bits in order:
    // bit 0 overall, then parity positions 1,2,4,8,16,32,64
    uint64_t p = 0;
    for (int i = 0; i < 8; ++i) {
        p |= (uint64_t(words[i].ecc) << (8 * i));
    }
    return p;
}

static inline void insert_parity_bits(array<EncodedWord, 8>& words, uint64_t p) {
    for (int i = 0; i < 8; ++i) {
        words[i].ecc = uint8_t((p >> (8 * i)) & 0xFFULL);
    }
}

static inline void bind_encoded_words(array<EncodedWord, 8>& words, uint64_t block_id) {
    uint64_t p = extract_parity_bits(words);
    p ^= block_id;
    insert_parity_bits(words, p);
}

static inline void unbind_encoded_words(array<EncodedWord, 8>& words, uint64_t block_id) {
    uint64_t p = extract_parity_bits(words);
    p ^= block_id;
    insert_parity_bits(words, p);
}

// -----------------------------
// Whole-block helpers
// -----------------------------
static inline array<EncodedWord, 8> encode_block(const array<uint64_t, 8>& data_words) {
    array<EncodedWord, 8> out{};
    for (int i = 0; i < 8; ++i) {
        out[i] = encode_word(data_words[i]);
    }
    return out;
}

static inline ReadResult read_and_decode(const array<EncodedWord, 8>& stored_encoded_words,
                                         uint64_t read_block_id) {
    array<EncodedWord, 8> work = stored_encoded_words;

    // Unbind
    unbind_encoded_words(work, read_block_id);

    ReadResult result{};

    for (int i = 0; i < 8; ++i) {
        try {
            DecodeResult dr = decode_word(work[i]);
            result.decoded_words[i] = dr.data;
            result.statuses[i] = dr.status;
        } catch (...) {
            result.ecc_error = true;
            result.statuses[i] = "uncorrectable";
            result.decoded_words[i] = 0; // placeholder
        }
    }

    return result;
}

// -----------------------------
// Block ID mixer
// -----------------------------
static inline uint64_t mix64(uint64_t z) {
    z &= MASK64;
    z = ((z ^ (z >> 33)) * 0xff51afd7ed558ccdULL) & MASK64;
    z = ((z ^ (z >> 33)) * 0xc4ceb9fe1a85ec53ULL) & MASK64;
    z = (z ^ (z >> 33)) & MASK64;
    return z;
}

static inline uint64_t wrap_block_id(uint32_t block_id30) {
    uint64_t x = uint64_t(block_id30 & ((1u << 30) - 1));
    return mix64(x);
}

// -----------------------------
// Random helpers
// -----------------------------
static inline uint64_t rand_u64(std::mt19937_64& rng) {
    return rng();
}

static inline uint32_t rand_u30(std::mt19937_64& rng) {
    return uint32_t(rng() & ((1ULL << 30) - 1));
}

static inline int rand_int(std::mt19937_64& rng, int lo, int hi_inclusive) {
    std::uniform_int_distribution<int> dist(lo, hi_inclusive);
    return dist(rng);
}

// Flip one of the 72 stored bits
// 0 = overall parity
// 1,2,4,8,16,32,64 = Hamming parity bits
// others = data bits
static inline void flip_encoded_bit(EncodedWord& w, int bit_index) {
    if (bit_index == 0) {
        w.ecc ^= 0x1;
        return;
    }

    bool is_power_of_two = (bit_index & (bit_index - 1)) == 0;
    if (is_power_of_two) {
        int p_idx = 0;
        while ((1 << p_idx) != bit_index) ++p_idx;
        w.ecc ^= uint8_t(1U << (p_idx + 1));
        return;
    }

    int data_bit = ENCODED_TO_DATA[bit_index];
    if (data_bit >= 0) {
        w.data ^= (1ULL << data_bit);
        return;
    }

    throw std::runtime_error("Bad bit index");
}
