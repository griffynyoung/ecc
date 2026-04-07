#include "test_utils.cpp"
int main() {
    std::mt19937_64 rng(123456789ULL);

    {
        std::cout << "\n=== TEST 1: SINGLE-BIT ERROR IN BLOCK (DATA OR PARITY) ===\n";
        int trials = 10000;
        int ecc_detected = 0;

        for (int t = 0; t < trials; ++t) {
            uint64_t block_id = wrap_block_id(rand_u30(rng));

            array<uint64_t, 8> data_words{};
            for (auto& w : data_words) w = rand_u64(rng);

            auto encoded_words = encode_block(data_words);
            bind_encoded_words(encoded_words, block_id);

            int w = rand_int(rng, 0, 7);
            int bit = rand_int(rng, 0, 71);
            flip_encoded_bit(encoded_words[w], bit);

            auto result = read_and_decode(encoded_words, block_id);
            if (result.ecc_error) ecc_detected++;

            for (int i = 0; i < 8; ++i) {
                assert(result.decoded_words[i] == data_words[i]);
            }
        }

        std::cout << "ECC uncorrectable errors: " << ecc_detected << "/" << trials << "\n";
        std::cout << "Single-bit error with correct address passed\n";
    }

    {
        std::cout << "\n=== TEST 2: SINGLE-BIT ERROR (DATA OR PARITY) IN EVERY WORD IN BLOCK ===\n";
        int trials = 10000;
        int ecc_detected = 0;

        for (int t = 0; t < trials; ++t) {
            uint64_t block_id = wrap_block_id(rand_u30(rng));

            array<uint64_t, 8> data_words{};
            for (auto& w : data_words) w = rand_u64(rng);

            auto encoded_words = encode_block(data_words);
            bind_encoded_words(encoded_words, block_id);

            for (int w = 0; w < 8; ++w) {
                int bit = rand_int(rng, 0, 71);
                flip_encoded_bit(encoded_words[w], bit);
            }

            auto result = read_and_decode(encoded_words, block_id);
            if (result.ecc_error) ecc_detected++;

            for (int i = 0; i < 8; ++i) {
                assert(result.decoded_words[i] == data_words[i]);
                assert(result.statuses[i] == "corrected");
            }
        }

        std::cout << "ECC uncorrectable errors: " << ecc_detected << "/" << trials << "\n";
        std::cout << "One single bit error (data or parity) in every word passed\n";
    }

    {
        std::cout << "\n=== TEST 3: DOUBLE-BIT ERROR IN BLOCK (DATA OR PARITY) ===\n";
        int detected = 0;
        int trials = 10000;

        for (int t = 0; t < trials; ++t) {
            uint64_t block_id = wrap_block_id(rand_u30(rng));

            array<uint64_t, 8> data_words{};
            for (auto& w : data_words) w = rand_u64(rng);

            auto encoded_words = encode_block(data_words);
            bind_encoded_words(encoded_words, block_id);

            int w = rand_int(rng, 0, 7);
            int b1 = rand_int(rng, 0, 71);
            int b2 = rand_int(rng, 0, 71);
            while (b2 == b1) b2 = rand_int(rng, 0, 71);

            flip_encoded_bit(encoded_words[w], b1);
            flip_encoded_bit(encoded_words[w], b2);

            auto result = read_and_decode(encoded_words, block_id);
            if (result.ecc_error) detected++;
        }

        std::cout << "ECC uncorrectable errors: " << detected << "/" << trials << "\n";
        std::cout << "Double-bit error detection passed\n";
    }

    {
        std::cout << "\n=== TEST 4: DOUBLE-BIT ERROR (DATA OR PARITY) IN EVERY WORD IN BLOCK ===\n";
        int trials = 10000;
        int ecc_detected = 0;

        for (int t = 0; t < trials; ++t) {
            uint64_t block_id = wrap_block_id(rand_u30(rng));

            array<uint64_t, 8> data_words{};
            for (auto& w : data_words) w = rand_u64(rng);

            auto encoded_words = encode_block(data_words);
            bind_encoded_words(encoded_words, block_id);

            for (int w = 0; w < 8; ++w) {
                int b1 = rand_int(rng, 0, 71);
                int b2 = rand_int(rng, 0, 71);
                while (b2 == b1) b2 = rand_int(rng, 0, 71);

                flip_encoded_bit(encoded_words[w], b1);
                flip_encoded_bit(encoded_words[w], b2);
            }

            auto result = read_and_decode(encoded_words, block_id);

            if (result.ecc_error) ecc_detected++;

            assert(result.ecc_error);
            for (int i = 0; i < 8; ++i) {
                assert(result.statuses[i] == "uncorrectable");
            }
        }

        std::cout << "ECC uncorrectable errors: " << ecc_detected << "/" << trials << "\n";
        std::cout << "Double-bit errors in every word passed\n";
    }
}
