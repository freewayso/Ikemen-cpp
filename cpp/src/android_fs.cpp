#ifdef __ANDROID__
#include "android_fs.hpp"
#include <SDL.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <jni.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static void mkdirs(const std::string& p) {
  std::string cur;
  for (size_t i = 0; i <= p.size(); i++) {
    if (i == p.size() || p[i] == '/') {
      if (!cur.empty() && cur != "/") mkdir(cur.c_str(), 0755);
      if (i < p.size()) {
        if (cur.empty() || cur.back() != '/') cur.push_back('/');
      }
    } else
      cur.push_back(p[i]);
  }
}

static bool copyAsset(AAssetManager* am, const std::string& rel, const std::string& dest) {
  AAsset* a = AAssetManager_open(am, rel.c_str(), AASSET_MODE_STREAMING);
  if (!a) return false;
  auto slash = dest.find_last_of('/');
  if (slash != std::string::npos) mkdirs(dest.substr(0, slash));
  FILE* f = std::fopen(dest.c_str(), "wb");
  if (!f) {
    AAsset_close(a);
    return false;
  }
  char buf[16 * 1024];
  int n = 0;
  while ((n = AAsset_read(a, buf, sizeof(buf))) > 0) std::fwrite(buf, 1, (size_t)n, f);
  std::fclose(f);
  AAsset_close(a);
  return true;
}

bool AndroidPrepareDataRoot(std::string& outRoot) {
  const char* base = SDL_AndroidGetInternalStoragePath();
  if (!base || !base[0]) return false;
  outRoot = std::string(base) + "/ikemen";
  mkdir(outRoot.c_str(), 0755);
  static const char kAssetVer[] = "6";
  std::string verpath = outRoot + "/.assets_ver";
  bool skip = false;
  FILE* vf = std::fopen(verpath.c_str(), "rb");
  if (vf) {
    char old[16] = {};
    std::fread(old, 1, 15, vf);
    std::fclose(vf);
    skip = std::strcmp(old, kAssetVer) == 0;
  }
  if (skip) return true;

  JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
  jobject activity = (jobject)SDL_AndroidGetActivity();
  if (!env || !activity) return false;
  jclass cls = env->GetObjectClass(activity);
  jmethodID mid = env->GetMethodID(cls, "getAssets", "()Landroid/content/res/AssetManager;");
  jobject amj = mid ? env->CallObjectMethod(activity, mid) : nullptr;
  AAssetManager* am = amj ? AAssetManager_fromJava(env, amj) : nullptr;
  if (!am) {
    env->DeleteLocalRef(activity);
    if (cls) env->DeleteLocalRef(cls);
    return false;
  }

  AAsset* idx = AAssetManager_open(am, "asset_index.txt", AASSET_MODE_BUFFER);
  if (!idx) {
    env->DeleteLocalRef(amj);
    env->DeleteLocalRef(activity);
    env->DeleteLocalRef(cls);
    return false;
  }
  off_t len = AAsset_getLength(idx);
  std::string text((size_t)len, '\0');
  AAsset_read(idx, text.data(), (size_t)len);
  AAsset_close(idx);

  size_t pos = 0;
  while (pos < text.size()) {
    size_t nl = text.find('\n', pos);
    if (nl == std::string::npos) nl = text.size();
    std::string rel = text.substr(pos, nl - pos);
    if (!rel.empty() && rel.back() == '\r') rel.pop_back();
    if (!rel.empty() && rel != "asset_index.txt") copyAsset(am, rel, outRoot + "/" + rel);
    pos = nl + 1;
  }

  FILE* m = std::fopen(verpath.c_str(), "wb");
  if (m) {
    std::fwrite(kAssetVer, 1, std::strlen(kAssetVer), m);
    std::fclose(m);
  }

  env->DeleteLocalRef(amj);
  env->DeleteLocalRef(activity);
  env->DeleteLocalRef(cls);
  std::fprintf(stderr, "android assets ready at %s\n", outRoot.c_str());
  return true;
}
#endif
