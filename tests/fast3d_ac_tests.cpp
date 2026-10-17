#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <random>
#include <tuple>
#include <vector>

#include "fast/interpreter.h"
#include "fast/resource/factory/DisplayListFactory.h"
#include "fast/resource/type/DisplayList.h"
#include "ship/resource/File.h"
#include "ship/utils/binarytools/BinaryReader.h"
#include "ship/utils/binarytools/MemoryStream.h"

using TrinTri = std::tuple<uint32_t, uint32_t, uint32_t>;
using namespace Fast;

namespace {

uint32_t Field(uint32_t word, int shift, int width) {
    return (word >> shift) & ((1u << width) - 1);
}

size_t ReferenceTrin(const F3DGfx* gfx, std::vector<TrinTri>& out) {
    int faces = (int)Field((uint32_t)gfx[0].words.w0, 17, 7) + 1;
    bool firstPass = true;
    size_t word = 0;

    while (faces > 0) {
        const uint32_t w0 = (uint32_t)gfx[word].words.w0;
        const uint32_t w1 = (uint32_t)gfx[word].words.w1;
        word++;

        if (Field(w1, 0, 1) == 0) {
            out.emplace_back(Field(w1, 4, 5), Field(w1, 9, 5), Field(w1, 14, 5));
            if (--faces == 0) {
                break;
            }
            out.emplace_back(Field(w1, 19, 5), Field(w1, 24, 5), (Field(w0, 0, 2) << 3) | Field(w1, 29, 3));
            if (--faces == 0) {
                break;
            }
            out.emplace_back(Field(w0, 2, 5), Field(w0, 7, 5), Field(w0, 12, 5));
            if (--faces == 0) {
                break;
            }
            if (firstPass) {
                firstPass = false;
            } else {
                out.emplace_back(Field(w0, 17, 5), Field(w0, 22, 5), Field(w0, 27, 5));
                if (--faces == 0) {
                    break;
                }
            }
        } else {
            out.emplace_back(Field(w1, 1, 7), Field(w1, 8, 7), Field(w1, 15, 7));
            if (--faces == 0) {
                break;
            }
            out.emplace_back(Field(w1, 22, 7), (Field(w0, 0, 4) << 3) | Field(w1, 29, 3), Field(w0, 4, 7));
            if (--faces == 0) {
                break;
            }
            if (firstPass) {
                firstPass = false;
            } else {
                out.emplace_back(Field(w0, 11, 7), Field(w0, 18, 7), Field(w0, 25, 7));
                if (--faces == 0) {
                    break;
                }
            }
        }
    }

    return word;
}

void Collect(void* ctx, uint32_t v0, uint32_t v1, uint32_t v2) {
    static_cast<std::vector<TrinTri>*>(ctx)->emplace_back(v0, v1, v2);
}

uint32_t Data1(uint32_t v0, uint32_t v1, uint32_t v2) {
    return ((v2 & 0x1F) << 10) | ((v1 & 0x1F) << 5) | (v0 & 0x1F);
}

F3DGfx TrinInit5b(uint32_t n, const uint32_t v[9]) {
    F3DGfx g;
    g.words.w0 = (0x0Au << 24) | (((n - 1) & 0x7F) << 17) | (Data1(v[6], v[7], v[8]) << 2) |
                 ((Data1(v[3], v[4], v[5]) >> 13) & 3);
    g.words.w1 = ((Data1(v[3], v[4], v[5]) & 0x1FFF) << 19) | (Data1(v[0], v[1], v[2]) << 4);
    return g;
}

F3DGfx Trin5b(const uint32_t v[12]) {
    F3DGfx g;
    g.words.w0 =
        (Data1(v[9], v[10], v[11]) << 17) | (Data1(v[6], v[7], v[8]) << 2) | ((Data1(v[3], v[4], v[5]) >> 13) & 3);
    g.words.w1 = ((Data1(v[3], v[4], v[5]) & 0x1FFF) << 19) | (Data1(v[0], v[1], v[2]) << 4);
    return g;
}

std::shared_ptr<Ship::File> MakeDisplayListFile(const std::vector<char>& body) {
    auto file = std::make_shared<Ship::File>();
    file->Buffer = std::make_shared<std::vector<char>>(body);
    file->IsLoaded = true;
    auto reader = std::make_shared<Ship::BinaryReader>(std::make_shared<Ship::MemoryStream>(file->Buffer));
    reader->SetEndianness(Ship::Endianness::Native);
    file->Reader = reader;
    return file;
}

std::shared_ptr<Ship::ResourceInitData> MakeInitData(int32_t version) {
    auto initData = std::make_shared<Ship::ResourceInitData>();
    initData->Format = RESOURCE_FORMAT_BINARY;
    initData->ByteOrder = Ship::Endianness::Native;
    initData->ResourceVersion = version;
    initData->Path = "test/dl";
    return initData;
}

void PutU32(std::vector<char>& out, uint32_t value) {
    out.insert(out.end(), reinterpret_cast<char*>(&value), reinterpret_cast<char*>(&value) + sizeof(value));
}

} // namespace

TEST(Fast3dAcTrin, MatchesEmu64ForEncodedModels) {
    std::mt19937 rng(0xAC64);
    std::uniform_int_distribution<uint32_t> index(0, 31);

    for (int iter = 0; iter < 10000; iter++) {
        const uint32_t n = 1 + rng() % 128;
        std::vector<F3DGfx> dl;
        uint32_t v[12];
        for (auto& x : v) {
            x = index(rng);
        }
        dl.push_back(TrinInit5b(n, v));
        for (int remaining = (int)n - 3; remaining > 0; remaining -= 4) {
            for (auto& x : v) {
                x = index(rng);
            }
            dl.push_back(Trin5b(v));
        }
        F3DGfx sentinel;
        sentinel.words.w0 = (uint32_t)(uint8_t)F3DEX2_G_ENDDL << 24;
        sentinel.words.w1 = 0;
        dl.push_back(sentinel);

        std::vector<TrinTri> expected;
        std::vector<TrinTri> actual;
        const size_t expectedWords = ReferenceTrin(dl.data(), expected);
        const size_t actualWords = Fast::GfxTrinDecodeAc(dl.data(), Collect, &actual);

        ASSERT_EQ(actualWords, expectedWords) << "n=" << n;
        ASSERT_EQ(actualWords, dl.size() - 1) << "n=" << n;
        ASSERT_EQ(actual, expected) << "n=" << n;
        ASSERT_EQ(actual.size(), n);
    }
}

TEST(Fast3dAcTrin, MatchesEmu64ForRawWords) {
    std::mt19937 rng(0x64AC);

    for (int iter = 0; iter < 10000; iter++) {
        std::vector<F3DGfx> dl(64);
        for (auto& g : dl) {
            g.words.w0 = rng();
            g.words.w1 = rng();
        }
        dl[0].words.w0 = (dl[0].words.w0 & 0x01FFFFFF) | (0x0Au << 24);

        std::vector<TrinTri> expected;
        std::vector<TrinTri> actual;
        ASSERT_EQ(Fast::GfxTrinDecodeAc(dl.data(), Collect, &actual), ReferenceTrin(dl.data(), expected));
        ASSERT_EQ(actual, expected);
    }
}

TEST(Fast3dAcTrin, WordCounts) {
    const uint32_t v[12] = {};
    for (uint32_t n = 1; n <= 128; n++) {
        std::vector<F3DGfx> dl(40);
        dl[0] = TrinInit5b(n, v);
        for (size_t i = 1; i < dl.size(); i++) {
            dl[i] = Trin5b(v);
        }
        const size_t expected = n <= 3 ? 1 : 1 + (n - 3 + 3) / 4;
        EXPECT_EQ(Fast::GfxTrinDecodeAc(dl.data(), nullptr, nullptr), expected) << "n=" << n;
    }
}

TEST(Fast3dAcTrin, InitWordSecondTriangleHighBits) {
    const uint32_t v[9] = { 16, 17, 18, 16, 18, 19, 0, 0, 0 };
    F3DGfx dl[1] = { TrinInit5b(2, v) };

    std::vector<TrinTri> actual;
    Fast::GfxTrinDecodeAc(dl, Collect, &actual);
    ASSERT_EQ(actual.size(), 2u);
    EXPECT_EQ(actual[0], TrinTri(16, 17, 18));
    EXPECT_EQ(actual[1], TrinTri(16, 18, 19));
}

TEST(Fast3dAcFog, MatchesEmu64Formula) {
    const double n = 30.0;
    const double f = 3000.0;
    float projection[4][4] = {};
    projection[0][0] = 1.0f;
    projection[1][1] = 1.0f;
    projection[2][2] = (float)((n + f) / (n - f));
    projection[2][3] = -1.0f;
    projection[3][2] = (float)(2.0 * n * f / (n - f));

    const int16_t fogMul = 128000 / (1000 - 990);
    const int16_t fogOffset = (500 - 990) * 256 / (1000 - 990);

    float start = 0.0f;
    float end = 0.0f;
    ASSERT_TRUE(Fast::GfxAcGxFogRange(projection, fogMul, fogOffset, &start, &end));

    const double a = projection[2][2];
    const double b = projection[3][2];
    const double zNear = b * ((a + 1.0) / (a - 1.0) - 1.0) / 2.0;
    const double zFar = zNear * ((a - 1.0) / (a + 1.0) + 1.0);
    const double p22 = zNear / (zNear - zFar);
    const double p23 = zNear * zFar / (zNear - zFar);
    const int fogMin = 500 - (fogOffset * 500) / fogMul;
    const int fogMax = 128000 / fogMul + fogMin;
    const double expectedStart = p23 / (((double)(uint32_t)fogMin - 1000.0) / 1064.0 + p22);
    const double expectedEnd = p23 / (((double)(uint32_t)fogMax - 1000.0) / 1016.0 + p22);

    EXPECT_NEAR(zNear, n, 1e-2);
    EXPECT_NEAR(start, expectedStart, std::abs(expectedStart) * 1e-4);
    EXPECT_NEAR(end, expectedEnd, std::abs(expectedEnd) * 1e-4);
    EXPECT_LT(start, end);
}

TEST(Fast3dAcFog, RejectsOrthoAndDisabledFog) {
    float ortho[4][4] = {};
    ortho[0][0] = ortho[1][1] = ortho[2][2] = ortho[3][3] = 1.0f;
    float start = 0.0f;
    float end = 0.0f;
    EXPECT_FALSE(Fast::GfxAcGxFogRange(ortho, 100, 0, &start, &end));

    float persp[4][4] = {};
    persp[2][2] = -1.02f;
    persp[2][3] = -1.0f;
    persp[3][2] = -60.0f;
    EXPECT_FALSE(Fast::GfxAcGxFogRange(persp, 0, 0, &start, &end));
}

TEST(Fast3dDisplayList, V0StopsAtEndOfFile) {
    std::vector<char> body;
    body.push_back((char)ucode_f3dex2_ac);
    while (body.size() % 8 != 0) {
        body.push_back(0);
    }
    for (int i = 0; i < 3; i++) {
        PutU32(body, (uint32_t)(uint8_t)F3DEX2_G_SPNOOP << 24);
        PutU32(body, 0);
    }

    Fast::ResourceFactoryBinaryDisplayListV0 factory;
    auto dl =
        std::static_pointer_cast<Fast::DisplayList>(factory.ReadResource(MakeDisplayListFile(body), MakeInitData(0)));
    ASSERT_NE(dl, nullptr);
    EXPECT_EQ(dl->UCode, ucode_f3dex2_ac);
    ASSERT_EQ(dl->Instructions.size(), 4u);
    EXPECT_EQ((uint32_t)(dl->Instructions[3].words.w0 >> 24), (uint32_t)(uint8_t)F3DEX2_G_ENDDL);
}

TEST(Fast3dDisplayList, V0DropsTruncatedExpandedCommand) {
    std::vector<char> body;
    body.push_back((char)ucode_f3dex2);
    while (body.size() % 8 != 0) {
        body.push_back(0);
    }
    PutU32(body, (uint32_t)(uint8_t)F3DEX2_G_SPNOOP << 24);
    PutU32(body, 0);
    PutU32(body, (uint32_t)(uint8_t)OTR_G_DL_OTR_HASH << 24);
    PutU32(body, 0);

    Fast::ResourceFactoryBinaryDisplayListV0 factory;
    auto dl =
        std::static_pointer_cast<Fast::DisplayList>(factory.ReadResource(MakeDisplayListFile(body), MakeInitData(0)));
    ASSERT_NE(dl, nullptr);
    ASSERT_EQ(dl->Instructions.size(), 2u);
    EXPECT_EQ((uint32_t)(dl->Instructions[1].words.w0 >> 24), (uint32_t)(uint8_t)F3DEX2_G_ENDDL);
}

TEST(Fast3dDisplayList, V1ReadsExplicitCount) {
    std::vector<char> body;
    body.push_back((char)ucode_f3dex2);
    while (body.size() % 8 != 0) {
        body.push_back(0);
    }
    PutU32(body, 4);
    PutU32(body, 0);
    const uint32_t words[4][2] = {
        { (uint32_t)(uint8_t)F3DEX2_AC_G_TRIN_INDEPEND << 24 | (3 << 17), 0 },
        { (uint32_t)(uint8_t)F3DEX2_G_ENDDL << 24, 0x12345678 },
        { (uint32_t)(uint8_t)F3DEX2_G_SPNOOP << 24, 0 },
        { (uint32_t)(uint8_t)F3DEX2_G_ENDDL << 24, 0 },
    };
    for (const auto& w : words) {
        PutU32(body, w[0]);
        PutU32(body, w[1]);
    }
    PutU32(body, 0xFFFFFFFF);
    PutU32(body, 0xFFFFFFFF);

    Fast::ResourceFactoryBinaryDisplayListV1 factory;
    auto dl =
        std::static_pointer_cast<Fast::DisplayList>(factory.ReadResource(MakeDisplayListFile(body), MakeInitData(1)));
    ASSERT_NE(dl, nullptr);
    ASSERT_EQ(dl->Instructions.size(), 4u);
    EXPECT_EQ((uint32_t)dl->Instructions[1].words.w1, 0x12345678u);
    EXPECT_EQ((uint32_t)(dl->Instructions[3].words.w0 >> 24), (uint32_t)(uint8_t)F3DEX2_G_ENDDL);
}

TEST(Fast3dDisplayList, V1RejectsTruncatedFile) {
    std::vector<char> body;
    body.push_back((char)ucode_f3dex2);
    while (body.size() % 8 != 0) {
        body.push_back(0);
    }
    PutU32(body, 3);
    PutU32(body, 0);
    PutU32(body, (uint32_t)(uint8_t)F3DEX2_G_SPNOOP << 24);
    PutU32(body, 0);

    Fast::ResourceFactoryBinaryDisplayListV1 factory;
    EXPECT_EQ(factory.ReadResource(MakeDisplayListFile(body), MakeInitData(1)), nullptr);
}

TEST(Fast3dDisplayList, RejectsTruncatedHeader) {
    std::vector<char> body(8, 0);
    body[0] = (char)ucode_f3dex2_ac;

    Fast::ResourceFactoryBinaryDisplayListV1 v1;
    EXPECT_EQ(v1.ReadResource(MakeDisplayListFile(body), MakeInitData(1)), nullptr);

    Fast::ResourceFactoryBinaryDisplayListV0 v0;
    EXPECT_EQ(v0.ReadResource(MakeDisplayListFile({}), MakeInitData(0)), nullptr);
}

namespace {

void Rgba5551Reference(uint16_t c, uint8_t* out) {
    out[0] = (uint8_t)(((c >> 11) * 0xFF) / 0x1F);
    out[1] = (uint8_t)((((c >> 6) & 0x1F) * 0xFF) / 0x1F);
    out[2] = (uint8_t)((((c >> 1) & 0x1F) * 0xFF) / 0x1F);
    out[3] = (c & 1) ? 255 : 0;
}

void PutU16BE(std::vector<uint8_t>& buf, size_t entry, uint16_t c) {
    buf[entry * 2] = (uint8_t)(c >> 8);
    buf[entry * 2 + 1] = (uint8_t)c;
}

uint64_t HashTlutReference(const RDP* rdp) {
    uint64_t h = 1469598103934665603ull;
    for (int half = 0; half < 2; half++) {
        const uint8_t* p = rdp->palettes[half];
        h = (h ^ (p != nullptr ? 0x55u : 0xAAu)) * 1099511628211ull;
        if (p == nullptr) {
            continue;
        }
        for (int b = 0; b < 256; b++) {
            h = (h ^ p[b]) * 1099511628211ull;
        }
    }
    return h;
}

} // namespace

TEST(Fast3dTlut, Rgba5551StagingDecodesAsBefore) {
    auto rdp = std::make_unique<RDP>();
    std::vector<uint8_t> src(512);
    uint8_t got[256 * 4];
    uint8_t want[4];
    for (uint32_t base = 0; base < 0x10000; base += 256) {
        for (uint32_t e = 0; e < 256; e++) {
            PutU16BE(src, e, (uint16_t)(base + e));
        }
        GfxStageTlut(rdp.get(), 256, 256, src.data(), false);
        ASSERT_FALSE(rdp->palette_half_rgba32[0] || rdp->palette_half_rgba32[1]);
        ASSERT_EQ(rdp->palettes[0], rdp->palette_staging[0]);
        ASSERT_EQ(rdp->palettes[1], rdp->palette_staging[1]);
        ASSERT_EQ(rdp->palette_dram_addr[1], src.data() + 256);
        ASSERT_EQ(memcmp(rdp->palette_staging, src.data(), 512), 0);

        GfxTlutToRgba32(rdp.get(), 0, 256, got);
        for (uint32_t e = 0; e < 256; e++) {
            Rgba5551Reference((uint16_t)(base + e), want);
            ASSERT_EQ(memcmp(got + e * 4, want, 4), 0) << "color " << (base + e);
        }
    }
}

TEST(Fast3dTlut, Rgba32Ci8KeepsPartialAlpha) {
    auto rdp = std::make_unique<RDP>();
    std::vector<uint8_t> src(256 * 4);
    for (size_t b = 0; b < src.size(); b++) {
        src[b] = (uint8_t)(b * 7 + 3);
    }
    GfxStageTlut(rdp.get(), 256, 256, src.data(), true);
    EXPECT_TRUE(rdp->palette_half_rgba32[0]);
    EXPECT_TRUE(rdp->palette_half_rgba32[1]);
    EXPECT_EQ(rdp->palettes[0], rdp->palette_staging[0]);
    EXPECT_EQ(rdp->palettes[1], rdp->palette_staging[1]);
    EXPECT_EQ(rdp->palette_dram_addr[0], src.data());
    EXPECT_EQ(rdp->palette_dram_addr[1], src.data() + 512);

    uint8_t got[256 * 4];
    GfxTlutToRgba32(rdp.get(), 0, 256, got);
    EXPECT_EQ(memcmp(got, src.data(), sizeof(got)), 0);
}

TEST(Fast3dTlut, MixedCi4Banks) {
    auto rdp = std::make_unique<RDP>();
    uint8_t model[256][4] = {};

    auto load16 = [&](uint32_t bank, uint16_t seed) {
        std::vector<uint8_t> src(32);
        for (uint32_t e = 0; e < 16; e++) {
            uint16_t c = (uint16_t)(seed + e * 0x1111);
            PutU16BE(src, e, c);
            Rgba5551Reference(c, model[bank * 16 + e]);
        }
        GfxStageTlut(rdp.get(), 256 + bank * 16, 16, src.data(), false);
    };
    auto load32 = [&](uint32_t bank, uint8_t seed) {
        std::vector<uint8_t> src(64);
        for (uint32_t b = 0; b < 64; b++) {
            src[b] = (uint8_t)(seed + b * 13);
        }
        memcpy(model[bank * 16], src.data(), 64);
        GfxStageTlut(rdp.get(), 256 + bank * 16, 16, src.data(), true);
    };

    for (uint32_t bank = 0; bank < 16; bank++) {
        load16(bank, (uint16_t)(bank * 0x0F0F + 1));
    }
    load32(1, 0x10);
    EXPECT_TRUE(rdp->palette_half_rgba32[0]);
    EXPECT_FALSE(rdp->palette_half_rgba32[1]);
    load16(2, 0x1234);
    load32(12, 0x80);
    load16(13, 0x4321);
    EXPECT_TRUE(rdp->palette_half_rgba32[1]);

    uint8_t got[256][4];
    GfxTlutToRgba32(rdp.get(), 0, 256, got[0]);
    for (uint32_t e = 0; e < 256; e++) {
        EXPECT_EQ(memcmp(got[e], model[e], 4), 0) << "entry " << e;
    }

    uint8_t bank12[16][4];
    GfxTlutToRgba32(rdp.get(), 12 * 16, 16, bank12[0]);
    EXPECT_EQ(memcmp(bank12, model[12 * 16], sizeof(bank12)), 0);
}

TEST(Fast3dTlut, FullHalfRgba5551LoadClearsRgba32) {
    auto rdp = std::make_unique<RDP>();
    std::vector<uint8_t> src32(64, 0x42);
    std::vector<uint8_t> src16(512, 0xFF);

    GfxStageTlut(rdp.get(), 256 + 16 * 9, 16, src32.data(), true);
    EXPECT_FALSE(rdp->palette_half_rgba32[0]);
    EXPECT_TRUE(rdp->palette_half_rgba32[1]);
    EXPECT_EQ(rdp->palettes[0], nullptr);

    GfxStageTlut(rdp.get(), 256, 256, src16.data(), false);
    EXPECT_FALSE(rdp->palette_half_rgba32[0]);
    EXPECT_FALSE(rdp->palette_half_rgba32[1]);

    GfxStageTlut(rdp.get(), 256 + 16 * 9, 16, src32.data(), true);
    EXPECT_TRUE(rdp->palette_half_rgba32[1]);
    GfxStageTlut(rdp.get(), 0, 16, src16.data(), false);
    EXPECT_FALSE(rdp->palette_half_rgba32[1]);
    EXPECT_EQ(rdp->palettes[1], src16.data());
}

TEST(Fast3dTlut, HashMatchesRgba5551AndTracksRgba32) {
    auto rdp = std::make_unique<RDP>();
    EXPECT_EQ(GfxHashTlutContent(rdp.get()), HashTlutReference(rdp.get()));

    std::vector<uint8_t> src16(512);
    for (uint32_t e = 0; e < 256; e++) {
        PutU16BE(src16, e, (uint16_t)(e * 0x0101 + 7));
    }
    GfxStageTlut(rdp.get(), 256 + 16, 16, src16.data(), false);
    EXPECT_EQ(GfxHashTlutContent(rdp.get()), HashTlutReference(rdp.get()));
    GfxStageTlut(rdp.get(), 256, 256, src16.data(), false);
    const uint64_t hash16 = GfxHashTlutContent(rdp.get());
    EXPECT_EQ(hash16, HashTlutReference(rdp.get()));

    std::vector<uint8_t> src32(64);
    for (uint32_t b = 0; b < 64; b++) {
        src32[b] = (b % 4 == 3) ? 0x80 : (uint8_t)(b * 5);
    }
    GfxStageTlut(rdp.get(), 256 + 16, 16, src32.data(), true);
    const uint64_t hash32 = GfxHashTlutContent(rdp.get());
    EXPECT_NE(hash32, hash16);

    src32[3] = 0x81;
    GfxStageTlut(rdp.get(), 256 + 16, 16, src32.data(), true);
    EXPECT_NE(GfxHashTlutContent(rdp.get()), hash32);
    src32[3] = 0x80;
    GfxStageTlut(rdp.get(), 256 + 16, 16, src32.data(), true);
    EXPECT_EQ(GfxHashTlutContent(rdp.get()), hash32);

    auto other = std::make_unique<RDP>();
    std::vector<uint8_t> alt16 = src16;
    for (uint32_t b = 32; b < 64; b++) {
        alt16[b] ^= 0x5A;
    }
    GfxStageTlut(other.get(), 256, 256, alt16.data(), false);
    GfxStageTlut(other.get(), 256 + 16, 16, src32.data(), true);
    EXPECT_EQ(GfxHashTlutContent(other.get()), hash32);
}

TEST(Fast3dTlut, DecodeTileReadsMixedPalette) {
    auto rdp = std::make_unique<RDP>();
    std::vector<uint8_t> src16(512);
    for (uint32_t e = 0; e < 256; e++) {
        PutU16BE(src16, e, (uint16_t)((e * 257) ^ 0x5A5A));
    }
    GfxStageTlut(rdp.get(), 256, 256, src16.data(), false);
    std::vector<uint8_t> src32(64);
    for (uint32_t b = 0; b < 64; b++) {
        src32[b] = (uint8_t)(200 - b);
    }
    GfxStageTlut(rdp.get(), 256 + 48, 16, src32.data(), true);

    uint8_t want[4];
    const uint8_t ci4[8] = { 0x01, 0x23, 0x00, 0x00, 0x45, 0xEF, 0x00, 0x00 };
    const uint8_t ci4Index[8] = { 0, 1, 2, 3, 4, 5, 14, 15 };
    uint8_t out[8 * 4];
    ASSERT_TRUE(GfxDecodeTileToRgba32(G_IM_FMT_CI, G_IM_SIZ_4b, ci4, 4, 4, 2, rdp.get(), 3, out));
    for (int i = 0; i < 8; i++) {
        EXPECT_EQ(memcmp(out + i * 4, src32.data() + ci4Index[i] * 4, 4), 0) << "texel " << i;
    }
    ASSERT_TRUE(GfxDecodeTileToRgba32(G_IM_FMT_CI, G_IM_SIZ_4b, ci4, 4, 4, 2, rdp.get(), 2, out));
    for (int i = 0; i < 8; i++) {
        uint32_t e = 32 + ci4Index[i];
        Rgba5551Reference((uint16_t)((e * 257) ^ 0x5A5A), want);
        EXPECT_EQ(memcmp(out + i * 4, want, 4), 0) << "texel " << i;
    }

    const uint8_t ci8[6] = { 0, 48, 63, 64, 200, 255 };
    uint8_t out8[6 * 4];
    ASSERT_TRUE(GfxDecodeTileToRgba32(G_IM_FMT_CI, G_IM_SIZ_8b, ci8, 3, 3, 2, rdp.get(), 0, out8));
    for (int i = 0; i < 6; i++) {
        uint32_t e = ci8[i];
        if (e >= 48 && e < 64) {
            memcpy(want, src32.data() + (e - 48) * 4, 4);
        } else {
            Rgba5551Reference((uint16_t)((e * 257) ^ 0x5A5A), want);
        }
        EXPECT_EQ(memcmp(out8 + i * 4, want, 4), 0) << "texel " << i;
    }

    auto empty = std::make_unique<RDP>();
    EXPECT_FALSE(GfxDecodeTileToRgba32(G_IM_FMT_CI, G_IM_SIZ_8b, ci8, 3, 3, 2, empty.get(), 0, out8));
    EXPECT_FALSE(GfxDecodeTileToRgba32(G_IM_FMT_CI, G_IM_SIZ_4b, ci4, 4, 4, 2, empty.get(), 3, out));
}

TEST(Fast3dTlut, LoadTlutStagesRgba32Resources) {
    auto gfx = std::make_unique<Interpreter>();
    auto tex = std::make_shared<Fast::Texture>();
    tex->Type = TextureType::RGBA32bpp;
    tex->Width = 16;
    tex->Height = 1;
    tex->ImageDataSize = 16 * 4;
    tex->ImageData = new uint8_t[16 * 4];
    for (uint32_t b = 0; b < 16 * 4; b++) {
        tex->ImageData[b] = (uint8_t)(b * 3 + 1);
    }
    RawTexMetadata metadata;
    metadata.width = tex->Width;
    metadata.height = tex->Height;
    metadata.type = tex->Type;
    metadata.resource = tex;

    gfx->GfxDpSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, nullptr, 0, metadata, tex->ImageData);
    gfx->GfxDpSetTile(G_IM_FMT_RGBA, G_IM_SIZ_4b, 0, 256 + 16 * 2, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gfx->GfxDpLoadTlut(G_TX_LOADTILE, 15);
    EXPECT_TRUE(gfx->mRdp->palette_texture_dirty);
    EXPECT_TRUE(gfx->mRdp->palette_half_rgba32[0]);
    uint8_t got[16 * 4];
    GfxTlutToRgba32(gfx->mRdp, 32, 16, got);
    EXPECT_EQ(memcmp(got, tex->ImageData, sizeof(got)), 0);

    std::vector<uint8_t> raw16(32);
    uint8_t want[16][4];
    for (uint32_t e = 0; e < 16; e++) {
        PutU16BE(raw16, e, (uint16_t)(0xF001 + e * 0x0842));
        Rgba5551Reference((uint16_t)(0xF001 + e * 0x0842), want[e]);
    }
    RawTexMetadata none = {};
    gfx->GfxDpSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, nullptr, 0, none, raw16.data());
    gfx->GfxDpSetTile(G_IM_FMT_RGBA, G_IM_SIZ_4b, 0, 256 + 16 * 9, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gfx->GfxDpLoadTlut(G_TX_LOADTILE, 15);
    EXPECT_FALSE(gfx->mRdp->palette_half_rgba32[1]);
    EXPECT_EQ(memcmp(gfx->mRdp->palette_staging[1] + 16 * 2, raw16.data(), raw16.size()), 0);

    gfx->GfxDpSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, nullptr, 0, metadata, tex->ImageData);
    gfx->GfxDpSetTile(G_IM_FMT_RGBA, G_IM_SIZ_4b, 0, 256, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gfx->GfxDpLoadTlut(G_TX_LOADTILE, 255);
    EXPECT_TRUE(gfx->mRdp->palette_half_rgba32[0]);
    EXPECT_TRUE(gfx->mRdp->palette_half_rgba32[1]);
    EXPECT_EQ(gfx->mRdp->palette_dram_addr[1], tex->ImageData + 128 * 4);
    std::vector<uint8_t> ci8(256 * 4);
    GfxTlutToRgba32(gfx->mRdp, 0, 256, ci8.data());
    EXPECT_EQ(memcmp(ci8.data(), tex->ImageData, 16 * 4), 0);
    EXPECT_EQ(std::count(ci8.begin() + 16 * 4, ci8.end(), 0), (256 - 16) * 4);

    gfx->GfxDpSetTextureImage(G_IM_FMT_RGBA, G_IM_SIZ_16b, 1, nullptr, 0, none, raw16.data());
    gfx->GfxDpSetTile(G_IM_FMT_RGBA, G_IM_SIZ_4b, 0, 256 + 16 * 9, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gfx->GfxDpLoadTlut(G_TX_LOADTILE, 15);
    EXPECT_TRUE(gfx->mRdp->palette_half_rgba32[1]);
    GfxTlutToRgba32(gfx->mRdp, 16 * 9, 16, got);
    EXPECT_EQ(memcmp(got, want, sizeof(got)), 0);
}

namespace {

void FillRectNdc(uint32_t width, uint32_t height, int32_t ulx, int32_t uly, int32_t lrx, int32_t lry, float* out) {
    const float halfWidth = (float)(width / 2);
    const float halfHeight = (float)(height / 2);
    out[0] = ulx / (4.0f * halfWidth) - 1.0f;
    out[1] = -(uly / (4.0f * halfHeight)) + 1.0f;
    out[2] = (lrx + 4) / (4.0f * halfWidth) - 1.0f;
    out[3] = -((lry + 4) / (4.0f * halfHeight)) + 1.0f;
}

} // namespace

TEST(Fast3dFillRect, WidenKeeps320x240Margins) {
    const uint32_t sizes[][2] = { { 320, 240 }, { 64, 64 }, { 200, 100 }, { 320, 32 } };
    for (const auto& size : sizes) {
        int32_t ulx = 0, uly = 0, lrx = 0, lry = 0;
        GfxWidenFullScreenFill(size[0], size[1], &ulx, &uly, &lrx, &lry);
        SCOPED_TRACE(::testing::Message() << size[0] << "x" << size[1]);
        EXPECT_EQ(ulx, -1024);
        EXPECT_EQ(uly, -1024);
        EXPECT_EQ(lrx, 2048);
        EXPECT_EQ(lry, 2048);
    }
}

TEST(Fast3dFillRect, WidenScalesWithLargerTargets) {
    float reference[4];
    FillRectNdc(320, 240, -1024, -1024, 2048, 2048, reference);

    const uint32_t sizes[][2] = { { 640, 480 }, { 1280, 720 }, { 424, 240 }, { 641, 481 }, { 320, 480 } };
    for (const auto& size : sizes) {
        int32_t ulx = 0, uly = 0, lrx = 0, lry = 0;
        GfxWidenFullScreenFill(size[0], size[1], &ulx, &uly, &lrx, &lry);
        float ndc[4];
        FillRectNdc(size[0], size[1], ulx, uly, lrx, lry, ndc);
        SCOPED_TRACE(::testing::Message() << size[0] << "x" << size[1]);
        for (int i = 0; i < 4; i++) {
            EXPECT_NEAR(ndc[i], reference[i], 0.01f);
        }
    }

    int32_t ulx = 0, uly = 0, lrx = 0, lry = 0;
    GfxWidenFullScreenFill(640, 480, &ulx, &uly, &lrx, &lry);
    EXPECT_EQ(ulx, -2048);
    EXPECT_EQ(uly, -2048);
    EXPECT_EQ(lrx, 4096);
    EXPECT_EQ(lry, 4096);

    float fixed640[4];
    FillRectNdc(640, 480, -1024, -1024, 2048, 2048, fixed640);
    EXPECT_NEAR((fixed640[2] + 1.0f) / 2.0f, 513.0f / 640.0f, 0.001f);
}

TEST(Fast3dLoadBlock, StockBlockUsesRenderTileLine) {
    const uint32_t width = 384, height = 104;
    std::vector<uint8_t> image(width / 2 * height);
    auto gfx = std::make_unique<Interpreter>();
    RawTexMetadata none = {};
    gfx->GfxDpSetTextureImage(G_IM_FMT_I, G_IM_SIZ_16b, 1, nullptr, 0, none, image.data());
    gfx->GfxDpSetTile(G_IM_FMT_I, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gfx->GfxDpLoadBlock(G_TX_LOADTILE, 0, 0, ((width * height + 3) >> 2) - 1, ((1 << 11) + 24 - 1) / 24);
    gfx->GfxDpSetTile(G_IM_FMT_I, G_IM_SIZ_4b, ((width >> 1) + 7) >> 3, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);

    const auto& loaded = gfx->mRdp->loaded_texture[gfx->mRdp->texture_tile[G_TX_RENDERTILE].tmem_index];
    EXPECT_EQ(loaded.size_bytes, image.size());
    EXPECT_EQ(loaded.line_size_bytes, loaded.size_bytes);
    EXPECT_EQ(loaded.full_image_line_size_bytes, loaded.size_bytes);
    const uint32_t line = gfx->mRdp->texture_tile[G_TX_RENDERTILE].line_size_bytes;
    EXPECT_EQ(line, width / 2);
    EXPECT_EQ(loaded.size_bytes / line, height);

    const uint32_t unpaddedWidth = 376;
    gfx->GfxDpLoadBlock(G_TX_LOADTILE, 0, 0, ((unpaddedWidth * height + 3) >> 2) - 1, ((1 << 11) + 23 - 1) / 23);
    EXPECT_EQ(loaded.size_bytes / line, 101u);
}

TEST(Fast3dLoadBlock, LoadTileKeepsPackedRows) {
    const uint32_t width = 376, height = 104;
    std::vector<uint8_t> image(width / 2 * height);
    auto gfx = std::make_unique<Interpreter>();
    RawTexMetadata none = {};
    const uint32_t line = ((width >> 1) + 7) >> 3;
    gfx->GfxDpSetTextureImage(G_IM_FMT_I, G_IM_SIZ_8b, width / 2, nullptr, 0, none, image.data());
    gfx->GfxDpSetTile(G_IM_FMT_I, G_IM_SIZ_8b, line, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gfx->GfxDpLoadTile(G_TX_LOADTILE, 0, 0, (width - 1) << 1, (height - 1) << 2);
    gfx->GfxDpSetTile(G_IM_FMT_I, G_IM_SIZ_4b, line, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);

    const auto& loaded = gfx->mRdp->loaded_texture[gfx->mRdp->texture_tile[G_TX_RENDERTILE].tmem_index];
    EXPECT_EQ(loaded.size_bytes, image.size());
    EXPECT_EQ(loaded.line_size_bytes, width / 2);
    EXPECT_EQ(loaded.full_image_line_size_bytes, width / 2);
    EXPECT_EQ(gfx->mRdp->texture_tile[G_TX_RENDERTILE].line_size_bytes, 192u);
}
