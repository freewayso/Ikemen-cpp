#include "sff.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <zlib.h>

static uint16_t ru16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
static uint32_t ru32(const uint8_t* p) { return uint32_t(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24)); }
static int16_t ri16(const uint8_t* p) { return int16_t(ru16(p)); }

static bool decodePcx(const uint8_t* data, size_t len, int16_t ax, int16_t ay, SpriteImage& out) {
  if (len < 128) return false;
  int xmin = ru16(data + 4), ymin = ru16(data + 6), xmax = ru16(data + 8), ymax = ru16(data + 10);
  int w = xmax - xmin + 1, h = ymax - ymin + 1;
  if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return false;
  int bpl = ru16(data + 66);
  if (bpl < w) bpl = w;
  std::vector<uint8_t> idx((size_t)w * h, 0);
  size_t i = 128;
  int x = 0, y = 0;
  while (y < h && i < len) {
    uint8_t b = data[i++];
    int count = 1;
    if ((b & 0xC0) == 0xC0) {
      count = b & 0x3F;
      if (i >= len) break;
      b = data[i++];
    }
    while (count-- > 0 && y < h) {
      if (x < w) idx[(size_t)y * w + x] = b;
      x++;
      if (x >= bpl) { x = 0; y++; }
    }
  }
  uint8_t pal[768] = {};
  if (len >= 769) {
    size_t p = len - 768;
    if (p > 0 && data[p - 1] == 0x0C) std::memcpy(pal, data + p, 768);
  }
  out.w = w; out.h = h; out.ax = ax; out.ay = ay;
  out.rgba.resize((size_t)w * h * 4);
  for (int p = 0; p < w * h; p++) {
    uint8_t c = idx[p];
    out.rgba[p * 4 + 0] = pal[c * 3];
    out.rgba[p * 4 + 1] = pal[c * 3 + 1];
    out.rgba[p * 4 + 2] = pal[c * 3 + 2];
    out.rgba[p * 4 + 3] = (c == 0) ? 0 : 255;
  }
  return true;
}

// Port of src/image.go Sprite.Lz5Decode
static std::vector<uint8_t> lz5(const uint8_t* rle, size_t rleLen, int w, int h) {
  if (w <= 0 || h <= 0 || (size_t)w * (size_t)h > 4000000u) return {};
  std::vector<uint8_t> p((size_t)w * (size_t)h);
  if (p.empty() || rleLen == 0) return p;
  size_t i = 0, j = 0;
  uint8_t ct = rle[i];
  unsigned cts = 0;
  uint8_t rb = 0;
  unsigned rbc = 0;
  if (i < rleLen - 1) i++;
  while (j < p.size() && i < rleLen) {
    int d = int(rle[i]);
    if (i < rleLen - 1) i++;
    int n = 0;
    if (ct & uint8_t(1u << cts)) {
      if ((d & 0x3f) == 0) {
        if (i >= rleLen) break;
        d = (d << 2 | int(rle[i])) + 1;
        if (i < rleLen - 1) i++;
        if (i >= rleLen) break;
        n = int(rle[i]) + 2;
        if (i < rleLen - 1) i++;
      } else {
        rb |= uint8_t((d & 0xc0) >> rbc);
        rbc += 2;
        n = d & 0x3f;
        if (rbc < 8) {
          if (i >= rleLen) break;
          d = int(rle[i]) + 1;
          if (i < rleLen - 1) i++;
        } else {
          d = int(rb) + 1;
          rb = 0;
          rbc = 0;
        }
      }
      for (;;) {
        if (j < p.size()) {
          int src = (int)j - d;
          p[j] = src >= 0 ? p[src] : 0;
          j++;
        }
        n--;
        if (n < 0) break;
      }
    } else {
      if ((d & 0xe0) == 0) {
        if (i >= rleLen) break;
        n = int(rle[i]) + 8;
        if (i < rleLen - 1) i++;
      } else {
        n = d >> 5;
        d &= 0x1f;
      }
      for (; n > 0; n--) {
        if (j < p.size()) p[j++] = uint8_t(d);
      }
    }
    cts++;
    if (cts >= 8) {
      if (i >= rleLen) break;
      ct = rle[i];
      cts = 0;
      if (i < rleLen - 1) i++;
    }
  }
  return p;
}

static std::vector<uint8_t> rle8(const uint8_t* rle, size_t rleLen, int w, int h) {
  if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || !rle) return {};
  std::vector<uint8_t> p((size_t)w * (size_t)h);
  size_t i = 0, j = 0;
  while (j < p.size() && i < rleLen) {
    int n = 1;
    uint8_t d = rle[i++];
    if ((d & 0xC0) == 0x40 && i < rleLen) {
      n = d & 0x3F;
      d = rle[i++];
    }
    while (n-- > 0 && j < p.size()) p[j++] = d;
  }
  return p;
}

static uint32_t be32(const uint8_t* p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

static int paeth(int a, int b, int c) {
  int p = a + b - c;
  int pa = std::abs(p - a), pb = std::abs(p - b), pc = std::abs(p - c);
  if (pa <= pb && pa <= pc) return a;
  if (pb <= pc) return b;
  return c;
}

// PNG8 (color type 3) keeps indices so we can apply the SFF palette.
// stb_image expands using the PNG PLTE, which Elecbyte leaves all-black.
static bool decodePngIndexed(const uint8_t* data, size_t len, int& w, int& h, std::vector<uint8_t>& idx) {
  if (len >= 8 && data[0] != 0x89) {
    data += 4;
    len -= 4;
  }
  if (len < 16 || std::memcmp(data, "\x89PNG\r\n\x1a\n", 8) != 0) return false;
  int bit = 0, color = -1, inter = 1;
  w = h = 0;
  std::vector<uint8_t> idat;
  size_t p = 8;
  while (p + 12 <= len) {
    uint32_t cl = be32(data + p);
    if (p + 12 + cl > len) break;
    const uint8_t* typ = data + p + 4;
    const uint8_t* cd = data + p + 8;
    if (std::memcmp(typ, "IHDR", 4) == 0 && cl >= 13) {
      w = (int)be32(cd);
      h = (int)be32(cd + 4);
      bit = cd[8];
      color = cd[9];
      inter = cd[12];
    } else if (std::memcmp(typ, "IDAT", 4) == 0) {
      idat.insert(idat.end(), cd, cd + cl);
    } else if (std::memcmp(typ, "IEND", 4) == 0) {
      break;
    }
    p += 12 + cl;
  }
  if (color != 3 || bit != 8 || inter != 0 || w <= 0 || h <= 0 || w > 4096 || h > 4096) return false;
  uLongf destLen = (uLongf)h * (uLongf)(w + 1) + 32;
  std::vector<uint8_t> raw(destLen);
  if (uncompress(raw.data(), &destLen, idat.data(), (uLong)idat.size()) != Z_OK) return false;
  raw.resize(destLen);
  idx.assign((size_t)w * h, 0);
  size_t i = 0;
  std::vector<uint8_t> prev((size_t)w, 0);
  for (int y = 0; y < h; y++) {
    if (i + 1 + (size_t)w > raw.size()) return false;
    uint8_t filt = raw[i++];
    uint8_t* row = idx.data() + (size_t)y * w;
    for (int x = 0; x < w; x++) {
      uint8_t v = raw[i + (size_t)x];
      uint8_t a = x > 0 ? row[x - 1] : 0;
      uint8_t b = prev[x];
      uint8_t c = x > 0 ? prev[x - 1] : 0;
      switch (filt) {
        case 0: break;
        case 1: v = (uint8_t)(v + a); break;
        case 2: v = (uint8_t)(v + b); break;
        case 3: v = (uint8_t)(v + ((a + b) / 2)); break;
        case 4: v = (uint8_t)(v + paeth(a, b, c)); break;
        default: return false;
      }
      row[x] = v;
    }
    i += (size_t)w;
    std::memcpy(prev.data(), row, (size_t)w);
  }
  return true;
}

static SpriteImage fromIndexed(const std::vector<uint8_t>& idx, int w, int h, int ax, int ay,
                               const std::vector<uint8_t>& pal) {
  SpriteImage out;
  out.w = w; out.h = h; out.ax = ax; out.ay = ay;
  out.idx = idx;
  out.palidx = 0;
  out.rgba.resize((size_t)w * h * 4);
  for (int i = 0; i < w * h; i++) {
    uint8_t c = i < (int)idx.size() ? idx[i] : 0;
    int o = c * 4;
    uint8_t r = 0, g = 0, b = 0, a = 0;
    if (o + 3 < (int)pal.size()) { r = pal[o]; g = pal[o + 1]; b = pal[o + 2]; a = pal[o + 3]; }
    if (c == 0) a = 0;
    out.rgba[i * 4 + 0] = r;
    out.rgba[i * 4 + 1] = g;
    out.rgba[i * 4 + 2] = b;
    out.rgba[i * 4 + 3] = a;
  }
  return out;
}

static bool loadV1(const std::vector<uint8_t>& file, std::unordered_map<SpriteKey, SpriteImage, SpriteKeyHash>& sprites) {
  uint32_t nspr = ru32(file.data() + 20);
  uint32_t first = ru32(file.data() + 24);
  std::vector<SpriteKey> order;
  uint32_t off = first;
  for (uint32_t n = 0; n < nspr && off + 32 <= file.size(); n++) {
    const uint8_t* h = file.data() + off;
    uint32_t next = ru32(h);
    uint32_t dlen = ru32(h + 4);
    int16_t ax = ri16(h + 8), ay = ri16(h + 10);
    uint16_t g = ru16(h + 12), img = ru16(h + 14);
    uint16_t linked = ru16(h + 16);
    SpriteKey key{g, img};
    SpriteImage spr;
    if (linked != 0 && linked < order.size()) {
      auto it = sprites.find(order[linked]);
      if (it != sprites.end()) spr = it->second;
    } else {
      uint32_t dataOff = off + 32;
      uint32_t end = next && next > dataOff ? next : dataOff + dlen;
      if (end > file.size()) end = (uint32_t)file.size();
      if (dataOff < end) decodePcx(file.data() + dataOff, end - dataOff, ax, ay, spr);
    }
    sprites.insert_or_assign(key, std::move(spr));
    order.push_back(key);
    if (!next || next <= off) break;
    off = next;
  }
  return !sprites.empty();
}

static bool loadV2(const std::vector<uint8_t>& file, std::unordered_map<SpriteKey, SpriteImage, SpriteKeyHash>& sprites,
                   std::vector<std::vector<uint8_t>>& palsOut, const std::vector<uint16_t>* keepGroups) {
  uint32_t first = ru32(file.data() + 36);
  uint32_t nspr = ru32(file.data() + 40);
  uint32_t palOfs = ru32(file.data() + 44);
  uint32_t npal = ru32(file.data() + 48);
  uint32_t lofs = ru32(file.data() + 52);
  uint32_t tofs = ru32(file.data() + 60);
  if (nspr > 20000 || first >= file.size()) return false;
  if (npal > 1024) npal = 1024;

  std::vector<std::vector<uint8_t>> pals(npal);
  for (uint32_t i = 0; i < npal; i++) {
    uint32_t ho = palOfs + i * 16;
    if (ho + 16 > file.size()) break;
    uint16_t link = ru16(file.data() + ho + 6);
    uint32_t ofs = ru32(file.data() + ho + 8);
    uint32_t sz = ru32(file.data() + ho + 12);
    std::vector<uint8_t> pal(256 * 4, 0);
    if (sz == 0 && link < pals.size()) pal = pals[link];
    else {
      uint32_t po = lofs + ofs;
      uint32_t colors = sz / 4;
      if (colors > 256) colors = 256;
      for (uint32_t c = 0; c < colors && po + c * 4 + 3 < file.size(); c++) {
        pal[c * 4 + 0] = file[po + c * 4 + 0];
        pal[c * 4 + 1] = file[po + c * 4 + 1];
        pal[c * 4 + 2] = file[po + c * 4 + 2];
        pal[c * 4 + 3] = file[po + c * 4 + 3];
      }
      // Go ReadPalette: SFF 2.0.0.0 forces index0 clear / others opaque.
      // kfm.sff is 2.0.1.0 (verlo2=1) and already stores those alphas.
      pal[3] = 0;
    }
    pals[i] = std::move(pal);
  }

  std::vector<SpriteKey> order;
  for (uint32_t n = 0; n < nspr; n++) {
    uint32_t ho = first + n * 28;
    if (ho + 28 > file.size()) break;
    const uint8_t* h = file.data() + ho;
    uint16_t g = ru16(h), img = ru16(h + 2);
    uint16_t w = ru16(h + 4), ht = ru16(h + 6);
    int16_t ax = ri16(h + 8), ay = ri16(h + 10);
    uint16_t linked = ru16(h + 12);
    uint8_t fmt = h[14];
    uint32_t dataOfs = ru32(h + 16);
    uint32_t dataSz = ru32(h + 20);
    uint16_t palidx = ru16(h + 24);
    uint16_t flags = ru16(h + 26);
    bool skip = false;
    if (keepGroups && !keepGroups->empty()) {
      skip = true;
      for (uint16_t kg : *keepGroups)
        if (kg == g) { skip = false; break; }
    }
    if ((size_t)w * (size_t)ht > 4000000u) skip = true;
    if (flags & 1) dataOfs += tofs;
    else dataOfs += lofs;

    SpriteKey key{g, img};
    SpriteImage spr;
    if (!skip && dataSz == 0 && linked < order.size()) {
      auto it = sprites.find(order[linked]);
      if (it != sprites.end()) spr = it->second;
      spr.ax = ax; spr.ay = ay;
    } else if (!skip && dataOfs < file.size() && dataSz > 0) {
      const uint8_t* data = file.data() + dataOfs;
      size_t len = dataSz;
      if (dataOfs + len > file.size()) len = file.size() - dataOfs;
      std::vector<uint8_t> palUse = palidx < pals.size() ? pals[palidx] : (pals.empty() ? std::vector<uint8_t>{} : pals[0]);
      const auto& pal = palUse;
      if (fmt == 4 && len > 4) {
        auto idx = lz5(data + 4, len - 4, w, ht);
        spr = fromIndexed(idx, w, ht, ax, ay, pal);
        spr.palidx = palidx;
      } else if (fmt == 2 && len > 4) {
        auto idx = rle8(data + 4, len - 4, w, ht);
        spr = fromIndexed(idx, w, ht, ax, ay, pal);
        spr.palidx = palidx;
      } else if (fmt == 0) {
        std::vector<uint8_t> idx(data, data + std::min(len, (size_t)w * ht));
        spr = fromIndexed(idx, w, ht, ax, ay, pal);
      } else if (fmt == 10 || fmt == 11 || fmt == 12) {
        int pw = 0, ph = 0;
        std::vector<uint8_t> pngIdx;
        if (decodePngIndexed(data, len, pw, ph, pngIdx)) {
          spr = fromIndexed(pngIdx, pw, ph, ax, ay, pal);
          spr.palidx = palidx;
        } else {
          int comp = 0;
          const uint8_t* png = data;
          int pngLen = (int)len;
          if (len > 8 && data[0] != 0x89) {
            png = data + 4;
            pngLen = (int)len - 4;
          }
          unsigned char* px = stbi_load_from_memory(png, pngLen, &pw, &ph, &comp, 4);
          if (px) {
            spr.w = pw; spr.h = ph; spr.ax = ax; spr.ay = ay;
            spr.rgba.assign(px, px + (size_t)pw * ph * 4);
            stbi_image_free(px);
          }
        }
      }
    }
    sprites.insert_or_assign(key, std::move(spr));
    order.push_back(key);
  }
  palsOut = std::move(pals);
  return !sprites.empty();
}

bool Sff::Load(const std::string& path) {
  return Load(path, {});
}

bool Sff::Load(const std::string& path, const std::vector<uint16_t>& keepGroups) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), {});
  if (file.size() < 64) return false;
  if (std::memcmp(file.data(), "ElecbyteSpr", 11) != 0) return false;
  sprites_.clear();
  pals_.clear();
  remapCache_.clear();
  sprites_.rehash(64);
  uint8_t verhi = file[15];
  const std::vector<uint16_t>* keep = keepGroups.empty() ? nullptr : &keepGroups;
  bool ok = (verhi >= 2) ? loadV2(file, sprites_, pals_, keep) : loadV1(file, sprites_);
  size_t okImg = 0;
  for (auto& kv : sprites_)
    if (kv.second.w > 0) okImg++;
  std::fprintf(stderr, "SFF v%d loaded %zu/%zu decoded sprites from %s\n", verhi, okImg, sprites_.size(), path.c_str());
  return ok && okImg > 0;
}

const SpriteImage* Sff::Get(uint16_t group, uint16_t number, int palRemap) const {
  auto it = sprites_.find(SpriteKey{group, number});
  if (it == sprites_.end() || it->second.w <= 0) return nullptr;
  if (palRemap < 0 || it->second.idx.empty() || palRemap == (int)it->second.palidx)
    return &it->second;
  if (palRemap >= (int)pals_.size()) return &it->second;
  uint64_t key = (uint64_t)group << 32 | (uint64_t)number << 16 | (uint16_t)palRemap;
  auto cached = remapCache_.find(key);
  if (cached != remapCache_.end()) return cached->second.get();
  auto baked = std::make_unique<SpriteImage>(fromIndexed(it->second.idx, it->second.w, it->second.h,
                                                         it->second.ax, it->second.ay, pals_[palRemap]));
  baked->palidx = (uint16_t)palRemap;
  const SpriteImage* p = baked.get();
  remapCache_[key] = std::move(baked);
  return p;
}

bool Sff::LoadPng(const std::string& path, SpriteImage& out) {
  int w = 0, h = 0, n = 0;
  unsigned char* px = stbi_load(path.c_str(), &w, &h, &n, 4);
  if (!px || w <= 0 || h <= 0) {
    if (px) stbi_image_free(px);
    return false;
  }
  out.w = w;
  out.h = h;
  out.rgba.assign(px, px + (size_t)w * h * 4);
  stbi_image_free(px);
  return true;
}
