#ifndef MINIZ_DEFLATE_H
#define MINIZ_DEFLATE_H

#include <cstdint>
#include <vector>
#include <string>
#include <cstring>

namespace ZipUtils {

inline const uint32_t* getCRC32Table() {
    static uint32_t table[256];
    static bool initialized = false;
    if (!initialized) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j) {
                c = (c & 1) ? (0xEDB88320L ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        initialized = true;
    }
    return table;
}

inline uint32_t calcCRC32(const uint8_t* data, size_t length, uint32_t crc = 0xFFFFFFFF) {
    const uint32_t* table = getCRC32Table();
    for (size_t i = 0; i < length; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

struct ZipCryptoKey {
    uint32_t key0 = 0x12345678;
    uint32_t key1 = 0x23456789;
    uint32_t key2 = 0x34567890;

    void update(uint8_t c) {
        const uint32_t* table = getCRC32Table();
        key0 = table[(key0 ^ c) & 0xFF] ^ (key0 >> 8);
        key1 = (key1 + (key0 & 0xFF)) * 134775813 + 1;
        key2 = table[(key2 ^ (key1 >> 24)) & 0xFF] ^ (key2 >> 8);
    }

    void init(const std::string& password) {
        key0 = 0x12345678;
        key1 = 0x23456789;
        key2 = 0x34567890;
        for (char c : password) {
            update(static_cast<uint8_t>(c));
        }
    }

    uint8_t decryptByte() const {
        uint16_t temp = (key2 & 0xFFFF) | 2;
        return static_cast<uint8_t>(((temp * (temp ^ 1)) >> 8) & 0xFF);
    }

    uint8_t decrypt(uint8_t c) {
        uint8_t k = decryptByte();
        uint8_t plain = c ^ k;
        update(plain);
        return plain;
    }
};

class DeflateDecompressor {
public:
    static bool decompress(const uint8_t* src, size_t src_len, std::vector<uint8_t>& dst, size_t expected_dst_len) {
        dst.resize(expected_dst_len);
        if (expected_dst_len == 0) return true;

        BitReader reader(src, src_len);
        size_t out_pos = 0;
        bool bfinal = false;

        while (!bfinal) {
            bfinal = reader.readBits(1) != 0;
            int btype = reader.readBits(2);

            if (btype == 0) {
                reader.alignToByte();
                uint16_t len = reader.readBits(16);
                uint16_t nlen = reader.readBits(16);
                if ((len ^ 0xFFFF) != nlen) return false;
                for (int i = 0; i < len; ++i) {
                    if (out_pos >= dst.size()) dst.resize(out_pos + 4096);
                    dst[out_pos++] = reader.readBits(8);
                }
            } else if (btype == 1 || btype == 2) {
                HuffmanTree lit_tree, dist_tree;
                if (btype == 1) {
                    buildFixedTrees(lit_tree, dist_tree);
                } else {
                    if (!buildDynamicTrees(reader, lit_tree, dist_tree)) return false;
                }

                while (true) {
                    int val = lit_tree.decode(reader);
                    if (val < 0) return false;
                    if (val < 256) {
                        if (out_pos >= dst.size()) dst.resize(out_pos + 4096);
                        dst[out_pos++] = static_cast<uint8_t>(val);
                    } else if (val == 256) {
                        break;
                    } else if (val <= 285) {
                        static const uint16_t len_base[] = {
                            3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258
                        };
                        static const uint8_t len_extra[] = {
                            0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0
                        };
                        int idx = val - 257;
                        int length = len_base[idx] + reader.readBits(len_extra[idx]);

                        int dist_code = dist_tree.decode(reader);
                        if (dist_code < 0 || dist_code > 29) return false;

                        static const uint16_t dist_base[] = {
                            1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577
                        };
                        static const uint8_t dist_extra[] = {
                            0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13
                        };
                        int distance = dist_base[dist_code] + reader.readBits(dist_extra[dist_code]);

                        if (out_pos < static_cast<size_t>(distance)) return false;
                        for (int k = 0; k < length; ++k) {
                            if (out_pos >= dst.size()) dst.resize(out_pos + 4096);
                            dst[out_pos] = dst[out_pos - distance];
                            out_pos++;
                        }
                    } else {
                        return false;
                    }
                }
            } else {
                return false;
            }
        }
        dst.resize(out_pos);
        return true;
    }

private:
    class BitReader {
    public:
        BitReader(const uint8_t* d, size_t s) : data(d), size(s), bit_pos(0) {}
        uint32_t readBits(int count) {
            uint32_t result = 0;
            for (int i = 0; i < count; ++i) {
                size_t byte_idx = bit_pos >> 3;
                if (byte_idx >= size) return 0;
                int bit_idx = bit_pos & 7;
                if ((data[byte_idx] >> bit_idx) & 1) {
                    result |= (1 << i);
                }
                bit_pos++;
            }
            return result;
        }
        void alignToByte() {
            bit_pos = (bit_pos + 7) & ~7;
        }
    private:
        const uint8_t* data;
        size_t size;
        size_t bit_pos;
    };

    struct HuffmanTree {
        struct Node {
            int left = -1;
            int right = -1;
            int symbol = -1;
        };
        std::vector<Node> nodes;

        void build(const std::vector<uint8_t>& code_lengths) {
            nodes.clear();
            nodes.emplace_back();

            int max_len = 0;
            for (uint8_t l : code_lengths) if (l > max_len) max_len = l;
            if (max_len == 0) return;

            std::vector<int> bl_count(max_len + 1, 0);
            for (uint8_t l : code_lengths) if (l > 0) bl_count[l]++;

            std::vector<uint32_t> next_code(max_len + 1, 0);
            uint32_t code = 0;
            for (int bits = 1; bits <= max_len; ++bits) {
                code = (code + bl_count[bits - 1]) << 1;
                next_code[bits] = code;
            }

            for (size_t sym = 0; sym < code_lengths.size(); ++sym) {
                uint8_t len = code_lengths[sym];
                if (len == 0) continue;
                uint32_t c = next_code[len]++;
                int curr = 0;
                for (int bit = len - 1; bit >= 0; --bit) {
                    int b = (c >> bit) & 1;
                    int next = (b == 0) ? nodes[curr].left : nodes[curr].right;
                    if (next == -1) {
                        next = static_cast<int>(nodes.size());
                        nodes.emplace_back();
                        if (b == 0) nodes[curr].left = next;
                        else nodes[curr].right = next;
                    }
                    curr = next;
                }
                nodes[curr].symbol = static_cast<int>(sym);
            }
        }

        int decode(BitReader& reader) const {
            if (nodes.empty()) return -1;
            int curr = 0;
            while (nodes[curr].symbol < 0) {
                int b = reader.readBits(1);
                int next = (b == 0) ? nodes[curr].left : nodes[curr].right;
                if (next < 0) return -1;
                curr = next;
            }
            return nodes[curr].symbol;
        }
    };

    static void buildFixedTrees(HuffmanTree& lit, HuffmanTree& dist) {
        std::vector<uint8_t> lit_lens(288);
        for (int i = 0; i <= 143; ++i) lit_lens[i] = 8;
        for (int i = 144; i <= 255; ++i) lit_lens[i] = 9;
        for (int i = 256; i <= 279; ++i) lit_lens[i] = 7;
        for (int i = 280; i <= 287; ++i) lit_lens[i] = 8;
        lit.build(lit_lens);

        std::vector<uint8_t> dist_lens(32, 5);
        dist.build(dist_lens);
    }

    static bool buildDynamicTrees(BitReader& reader, HuffmanTree& lit, HuffmanTree& dist) {
        int hlit = reader.readBits(5) + 257;
        int hdist = reader.readBits(5) + 1;
        int hclen = reader.readBits(4) + 4;

        static const uint8_t cl_order[] = {
            16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
        };
        std::vector<uint8_t> cl_lens(19, 0);
        for (int i = 0; i < hclen; ++i) {
            cl_lens[cl_order[i]] = static_cast<uint8_t>(reader.readBits(3));
        }
        HuffmanTree cl_tree;
        cl_tree.build(cl_lens);

        std::vector<uint8_t> all_lens(hlit + hdist, 0);
        size_t idx = 0;
        while (idx < all_lens.size()) {
            int sym = cl_tree.decode(reader);
            if (sym < 0) return false;
            if (sym <= 15) {
                all_lens[idx++] = static_cast<uint8_t>(sym);
            } else if (sym == 16) {
                if (idx == 0) return false;
                uint8_t prev = all_lens[idx - 1];
                int rep = reader.readBits(2) + 3;
                while (rep-- > 0 && idx < all_lens.size()) all_lens[idx++] = prev;
            } else if (sym == 17) {
                int rep = reader.readBits(3) + 3;
                while (rep-- > 0 && idx < all_lens.size()) all_lens[idx++] = 0;
            } else if (sym == 18) {
                int rep = reader.readBits(7) + 11;
                while (rep-- > 0 && idx < all_lens.size()) all_lens[idx++] = 0;
            }
        }

        std::vector<uint8_t> lit_lens(all_lens.begin(), all_lens.begin() + hlit);
        std::vector<uint8_t> dist_lens(all_lens.begin() + hlit, all_lens.end());
        lit.build(lit_lens);
        dist.build(dist_lens);
        return true;
    }
};

} // namespace ZipUtils

#endif // MINIZ_DEFLATE_H
