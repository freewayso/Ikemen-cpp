#include "log.hpp"
#include <cstring>
#include <ctime>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

static unsigned nowMs() {
#ifdef _WIN32
  return GetTickCount();
#else
  return 0;
#endif
}

GameLog& GameLog::Get() {
  static GameLog g;
  return g;
}

bool GameLog::Open(const std::string& dir, const std::string& mode, bool fight) {
  Close();
#ifdef _WIN32
  pid_ = (unsigned)GetCurrentProcessId();
  std::string folder = dir.empty() ? std::string("logs") : (dir + "\\logs");
  CreateDirectoryA(folder.c_str(), nullptr);
  std::snprintf(path_, sizeof(path_), "%s\\ikemen_%u.log", folder.c_str(), pid_);
#else
  pid_ = (unsigned)getpid();
  std::string folder = dir.empty() ? std::string("logs") : (dir + "/logs");
  mkdir(folder.c_str(), 0755);
  std::snprintf(path_, sizeof(path_), "%s/ikemen_%u.log", folder.c_str(), pid_);
#endif
  fp_ = std::fopen(path_, "w");
  if (!fp_) return false;
  std::setvbuf(fp_, nullptr, _IOFBF, 64 * 1024);
  fight_ = fight;
  pending_ = 0;
  lastFlushMs_ = nowMs();
  std::time_t t = std::time(nullptr);
  char ts[32] = {};
  std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
  Info("open pid=%u mode=%s fight=%d time=%s", pid_, mode.c_str(), fight_ ? 1 : 0, ts);
  return true;
}

void GameLog::Close() {
  if (!fp_) return;
  Flush();
  std::fclose(fp_);
  fp_ = nullptr;
}

void GameLog::Flush() {
  if (fp_) {
    std::fflush(fp_);
    pending_ = 0;
    lastFlushMs_ = nowMs();
  }
}

void GameLog::write(const char* lvl, const char* fmt, va_list ap) {
  if (!fp_) return;
  char line[1024];
  int n = std::snprintf(line, sizeof(line), "[%s] ", lvl);
  if (n < 0) n = 0;
  if (n >= (int)sizeof(line)) n = (int)sizeof(line) - 1;
  int m = std::vsnprintf(line + n, sizeof(line) - (size_t)n, fmt, ap);
  if (m < 0) m = 0;
  int tot = n + m;
  if (tot >= (int)sizeof(line)) tot = (int)sizeof(line) - 1;
  if (tot > 0 && line[tot - 1] != '\n') {
    if (tot < (int)sizeof(line) - 1) line[tot++] = '\n';
    line[tot] = 0;
  }
  std::fwrite(line, 1, (size_t)tot, fp_);
  pending_++;
  unsigned t = nowMs();
  if (pending_ >= 48 || (t - lastFlushMs_) > 2000) Flush();
}

void GameLog::Info(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  write("info", fmt, ap);
  va_end(ap);
}
void GameLog::Warn(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  write("warn", fmt, ap);
  va_end(ap);
  if (fp_) std::fprintf(stderr, "warn: ");
  va_start(ap, fmt);
  std::vfprintf(stderr, fmt, ap);
  va_end(ap);
  std::fputc('\n', stderr);
}
void GameLog::Error(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  write("error", fmt, ap);
  va_end(ap);
  va_start(ap, fmt);
  std::vfprintf(stderr, fmt, ap);
  va_end(ap);
  std::fputc('\n', stderr);
}
void GameLog::Fight(const char* fmt, ...) {
  if (!fight_ || !fp_) return;
  va_list ap;
  va_start(ap, fmt);
  write("fight", fmt, ap);
  va_end(ap);
}
