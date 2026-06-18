// gen_geometry.cpp — Offline tool: generate all 8415 rectangles + metadata → data.bin
// Build: g++ -O3 -std=c++20 gen_geometry.cpp -o gen_geometry.exe
// Run:   ./gen_geometry.exe
// Output: data.bin (~1 MB)

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <array>

namespace {

constexpr int k_rows = 10;
constexpr int k_cols = 17;
constexpr int k_cells = k_rows * k_cols;
constexpr int k_num_row_pairs = k_rows * (k_rows + 1) / 2; // 55
constexpr int k_num_col_pairs = k_cols * (k_cols + 1) / 2; // 153
constexpr int k_num_rects = k_num_row_pairs * k_num_col_pairs; // 8415

struct Bitboard {
    std::uint64_t lo{0};
    std::uint64_t mid{0};
    std::uint64_t hi{0};

    void set(int idx) {
        if (idx < 64) lo |= (1ULL << idx);
        else if (idx < 128) mid |= (1ULL << (idx - 64));
        else hi |= (1ULL << (idx - 128));
    }
};

// Rect record in data.bin (104 bytes, packed)
#pragma pack(push, 1)
struct RectRecord {
    std::int8_t r1, c1, r2, c2; // 4 bytes
    std::uint16_t cell_count;    // 2 bytes
    std::uint16_t cell_offset;   // 2 bytes
    std::uint64_t top_lo, top_mid, top_hi;       // 24 bytes
    std::uint64_t bottom_lo, bottom_mid, bottom_hi;
    std::uint64_t left_lo, left_mid, left_hi;
    std::uint64_t right_lo, right_mid, right_hi;
};
#pragma pack(pop)

static_assert(sizeof(RectRecord) == 104, "RectRecord must be 104 bytes");

int row_pair_index(int r1, int r2) {
    int offset = r1 * k_rows - r1 * (r1 - 1) / 2;
    return offset + (r2 - r1);
}

int col_pair_index(int c1, int c2) {
    int offset = c1 * k_cols - c1 * (c1 - 1) / 2;
    return offset + (c2 - c1);
}

int rect_id(int r1, int c1, int r2, int c2) {
    return row_pair_index(r1, r2) * k_num_col_pairs + col_pair_index(c1, c2);
}

void build_border_mask(Bitboard& bb, int r1, int c1, int r2, int c2,
                       int edge_r1, int edge_r2, int edge_c1, int edge_c2) {
    for (int r = edge_r1; r <= edge_r2; ++r) {
        for (int c = edge_c1; c <= edge_c2; ++c) {
            bb.set(r * k_cols + c);
        }
    }
}

} // namespace

int main() {
    std::vector<std::uint8_t> cell_table;
    std::vector<RectRecord> rects(k_num_rects);

    // Generate all rects in rect_id order
    for (int r1 = 0; r1 < k_rows; ++r1) {
        for (int r2 = r1; r2 < k_rows; ++r2) {
            for (int c1 = 0; c1 < k_cols; ++c1) {
                for (int c2 = c1; c2 < k_cols; ++c2) {
                    int id = rect_id(r1, c1, r2, c2);
                    auto& rec = rects[id];

                    rec.r1 = static_cast<std::int8_t>(r1);
                    rec.c1 = static_cast<std::int8_t>(c1);
                    rec.r2 = static_cast<std::int8_t>(r2);
                    rec.c2 = static_cast<std::int8_t>(c2);

                    // Cell membership
                    int count = (r2 - r1 + 1) * (c2 - c1 + 1);
                    rec.cell_count = static_cast<std::uint16_t>(count);
                    rec.cell_offset = static_cast<std::uint16_t>(cell_table.size());

                    for (int r = r1; r <= r2; ++r) {
                        for (int c = c1; c <= c2; ++c) {
                            cell_table.push_back(static_cast<std::uint8_t>(r * k_cols + c));
                        }
                    }

                    // Border masks
                    Bitboard top, bottom, left, right;
                    build_border_mask(top, r1, c1, r2, c2, r1, r1, c1, c2);
                    build_border_mask(bottom, r1, c1, r2, c2, r2, r2, c1, c2);
                    build_border_mask(left, r1, c1, r2, c2, r1, r2, c1, c1);
                    build_border_mask(right, r1, c1, r2, c2, r1, r2, c2, c2);

                    rec.top_lo = top.lo; rec.top_mid = top.mid; rec.top_hi = top.hi;
                    rec.bottom_lo = bottom.lo; rec.bottom_mid = bottom.mid; rec.bottom_hi = bottom.hi;
                    rec.left_lo = left.lo; rec.left_mid = left.mid; rec.left_hi = left.hi;
                    rec.right_lo = right.lo; rec.right_mid = right.mid; rec.right_hi = right.hi;
                }
            }
        }
    }

    // Write data.bin
    FILE* f = fopen("data.bin", "wb");
    if (!f) { std::fprintf(stderr, "ERROR: Cannot create data.bin\n"); return 1; }

    // Header
    std::uint32_t magic = 0x43524459; // "CRDY"
    std::uint32_t num_rects = k_num_rects;
    std::uint32_t cell_table_size = static_cast<std::uint32_t>(cell_table.size());
    std::uint32_t checksum = 0;

    std::fwrite(&magic, 4, 1, f);
    std::fwrite(&num_rects, 4, 1, f);
    std::fwrite(&cell_table_size, 4, 1, f);
    std::fwrite(&checksum, 4, 1, f); // placeholder

    // Rect table
    std::fwrite(rects.data(), sizeof(RectRecord), k_num_rects, f);

    // Cell table
    std::fwrite(cell_table.data(), 1, cell_table.size(), f);

    std::fclose(f);

    // Verify
    long file_size = 16 + sizeof(RectRecord) * k_num_rects + cell_table.size();
    std::printf("Generated data.bin: %ld bytes (header=16, rects=%zu, cells=%zu)\n",
                file_size, sizeof(RectRecord) * k_num_rects, cell_table.size());
    std::printf("Rect count: %u\n", k_num_rects);
    std::printf("Rect ID test: (0,0,0,0)=%d, (9,16,9,16)=%d\n",
                rect_id(0,0,0,0), rect_id(9,16,9,16));

    return 0;
}
