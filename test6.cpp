// -----------------------------
// Main: sectioned Test 6
// Usage:
//   ./test6 SECTION_ID NUM_SECTIONS [BASE_SEED]
// Example:
//   ./test6 17 100 123456789
// -----------------------------
#include "test_utils.cpp"
int main(int argc, char** argv) {
    static constexpr uint64_t MASK64 = 0xFFFFFFFFFFFFFFFFULL;
    static constexpr uint32_t ADDRESS_BITS = 30;
    static constexpr uint64_t NUM_ADDRESSES = (1ULL << ADDRESS_BITS);

    std::cout << std::unitbuf;

    if (argc < 3 || argc > 4) {
        std::cerr << "Usage: " << argv[0]
                  << " SECTION_ID NUM_SECTIONS [BASE_SEED]\n";
        return 1;
    }

    const uint64_t section_id = std::strtoull(argv[1], nullptr, 10);
    const uint64_t num_sections = std::strtoull(argv[2], nullptr, 10);
    const uint64_t base_seed =
        (argc == 4) ? std::strtoull(argv[3], nullptr, 10) : 123456789ULL;

    if (num_sections == 0) {
        std::cerr << "NUM_SECTIONS must be > 0\n";
        return 1;
    }
    if (section_id >= num_sections) {
        std::cerr << "SECTION_ID must be in [0, NUM_SECTIONS)\n";
        return 1;
    }

    const uint64_t start_addr = (NUM_ADDRESSES * section_id) / num_sections;
    const uint64_t end_addr   = (NUM_ADDRESSES * (section_id + 1)) / num_sections;

    // Deterministic per-section seed
    const uint64_t seed_stride = 0x9e3779b97f4a7c15ULL;
    const uint64_t section_seed = base_seed + seed_stride * section_id;

    std::mt19937_64 rng(section_seed);

    std::cout << "\n=== TEST 6: TWO-BIT-OFF ADDRESS (SECTIONED) ===\n";
    std::cout << "SECTION_ID   = " << section_id << "\n";
    std::cout << "NUM_SECTIONS = " << num_sections << "\n";
    std::cout << "SECTION_SEED = " << section_seed << "\n";
    std::cout << "START_ADDR   = " << start_addr << "\n";
    std::cout << "END_ADDR     = " << end_addr << "\n";
    std::cout << "ADDR_COUNT   = " << (end_addr - start_addr) << "\n";

    using clock = std::chrono::steady_clock;
    auto test_start = clock::now();
    auto last_print = test_start;

    uint64_t ecc_detected = 0;
    uint64_t total_trials = 0;
    uint64_t ecc_threshold = 0;

    constexpr uint32_t PROGRESS_INTERVAL = (1u << 17); // 131072 addresses

    for (uint64_t true_id_raw = start_addr; true_id_raw < end_addr; ++true_id_raw) {
        uint64_t true_id = wrap_block_id(static_cast<uint32_t>(true_id_raw));

        for (uint32_t b1 = 0; b1 < ADDRESS_BITS; ++b1) {
            for (uint32_t b2 = b1 + 1; b2 < ADDRESS_BITS; ++b2) {
                uint32_t wrong_id_raw =
                    static_cast<uint32_t>(true_id_raw) ^ (1u << b1) ^ (1u << b2);
                uint64_t wrong_id = wrap_block_id(wrong_id_raw);

                array<uint64_t, 8> data_words{};
                for (auto& w : data_words) {
                    w = rand_u64(rng);
                }

                auto encoded_words = encode_block(data_words);
                bind_encoded_words(encoded_words, true_id);

                auto result = read_and_decode(encoded_words, wrong_id);

                total_trials++;

                if (result.ecc_error) {
                    ecc_detected++;
                } else {
                    int corrected_count = 0;
                    for (const auto& st : result.statuses) {
                        if (st == "corrected") {
                            corrected_count++;
                        }
                    }
                    if (corrected_count >= 4) {
                        ecc_threshold++;
                    }
                }
            }
        }

        const uint64_t processed = true_id_raw - start_addr;
        if ((processed % PROGRESS_INTERVAL) == 0) {
            auto now = clock::now();
            double elapsed_total =
                std::chrono::duration<double>(now - test_start).count();
            double elapsed_since_last =
                std::chrono::duration<double>(now - last_print).count();

            double pct = 100.0 * static_cast<double>(true_id_raw - start_addr)
                       / static_cast<double>(end_addr - start_addr);
            double rate = (elapsed_total > 0.0)
                        ? (static_cast<double>(total_trials) / elapsed_total)
                        : 0.0;

            std::cout << "[TEST 6 PROGRESS] address "
                      << true_id_raw << " / " << (end_addr - 1)
                      << " (" << std::fixed << std::setprecision(2) << pct << "%)"
                      << ", total_trials = " << total_trials
                      << ", elapsed = " << elapsed_total << " s"
                      << ", rate = " << std::setprecision(0) << rate << " trials/s"
                      << ", since_last = " << std::setprecision(2) << elapsed_since_last << " s"
                      << "\n";

            last_print = now;
        }
    }

    auto test_end = clock::now();
    double total_elapsed = std::chrono::duration<double>(test_end - test_start).count();

    std::cout << "ECC uncorrectable errors: " << ecc_detected << "/" << total_trials << "\n";
    std::cout << "ECC correction threshold met: " << ecc_threshold << "/" << total_trials << "\n";
    std::cout << "Section completed\n";
    std::cout << "TEST 6 total elapsed time: "
              << std::fixed << std::setprecision(2) << total_elapsed << " s\n";

    return 0;
}

