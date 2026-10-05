/**
 * @file Progress.cpp
 * @brief Terminal progress indicators, spinners, and progress bars implementation.
 */

#include "../../../include/Ksee/Terminal/Progress.hpp"
#include "../../../include/Ksee/Terminal/Color.hpp"
#include <stdio.h>

namespace Ksee {

static u64 getEpochMs() {
  return millis();
}

Progress::Progress() {}
Progress::~Progress() { destroy(); }

void Progress::update() {
  if (!_started) {
    _started = true;
    printf("\x1b[?25l");
    _hiddenCursor = true;
  }
  _render();
}

void Progress::destroy() {
  if (_started) {
    _started = false;
    int lines = (int)tasks.size();
    if (!message.isEmpty()) lines += 1;
    if (lines > 0) {
        printf("\x1b[%dB", lines);
    }
    printf("\n");
    if (_hiddenCursor) {
      printf("\x1b[?25h");
      _hiddenCursor = false;
    }
    fflush(stdout);
  }
}

usz Progress::addLinearTask(u64 total, const String& unit, const String& msg) {
  LinearTask t;
  t.totalRaw = total;
  t.unit = unit;
  t.message = msg;
  t.startTime = getEpochMs();
  t.lastUpdateTime = t.startTime;
  tasks.push(t);
  return tasks.size() - 1;
}

void Progress::updateLinearTask(usz taskIdx, u64 current, const String& msg) {
  if (taskIdx >= tasks.size()) return;
  auto& t = tasks[taskIdx];
  t.currentRaw = current;
  if (!msg.isEmpty()) t.message = msg;
  t.lastUpdateTime = getEpochMs();
}

void Progress::addLinearTaskDelta(usz taskIdx, u64 delta, const String& msg) {
  if (taskIdx >= tasks.size()) return;
  auto& t = tasks[taskIdx];
  t.currentRaw += delta;
  if (!msg.isEmpty()) t.message = msg;
  t.lastUpdateTime = getEpochMs();
}

String Progress::_formatSize(u64 bytes) {
  double val = (double)bytes;
  const char* units[] = {"B", "KB", "MB", "GB", "TB"};
  int uIdx = 0;
  while (val >= 1024.0 && uIdx < 4) {
    val /= 1024.0;
    uIdx++;
  }
  char buf[64];
  snprintf(buf, sizeof(buf), "%.1f %s", val, units[uIdx]);
  return String(buf);
}

String Progress::_formatTime(double seconds) {
  if (seconds < 0.0 || seconds > 3600.0 * 24.0 * 365.0) return "--:--";
  int h = (int)(seconds / 3600);
  int m = (int)((seconds - h * 3600) / 60);
  int s = (int)(seconds - h * 3600 - m * 60);
  char buf[64];
  if (h > 0) {
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
  } else {
    snprintf(buf, sizeof(buf), "%02d:%02d", m, s);
  }
  return String(buf);
}

void Progress::_render() {
  printf("\r\x1b[J");
  if (!message.isEmpty()) {
      printf("%s\n", message.c_str());
  }

  for (usz i = 0; i < tasks.size(); ++i) {
    auto &t = tasks[i];
    if (t.startTime == 0) t.startTime = getEpochMs();
    u64 now = getEpochMs();
    double elapsed = (now - t.startTime) / 1000.0;
    if (elapsed > 0.05) {
        t.speed = (double)t.currentRaw / elapsed;
    }
    double pct = (t.totalRaw > 0) ? (double)t.currentRaw / t.totalRaw : 0.0;
    if (pct > 1.0) pct = 1.0;
    double etaVal = 0.0;
    if (t.speed > 0.0 && t.totalRaw > t.currentRaw) {
        etaVal = (double)(t.totalRaw - t.currentRaw) / t.speed;
    }

    t.current = (t.unit == "B") ? _formatSize(t.currentRaw) : String::fromNumber((long long)t.currentRaw);
    t.total = (t.unit == "B") ? _formatSize(t.totalRaw) : String::fromNumber((long long)t.totalRaw);
    t.eta = _formatTime(elapsed) + " < " + _formatTime(etaVal);

    int barWidth = 25;
    int filled = (int)(pct * barWidth);
    String filledStr;
    for (int j = 0; j < filled; ++j) filledStr += "█";
    String emptyStr;
    for (int j = filled; j < barWidth; ++j) emptyStr += "░";

    String bar = "[" + Cyan(filledStr) + Gray(emptyStr) + "]";
    String pctText = String::fromNumber((int)(pct * 100)) + "%";

    String speedText;
    if (t.speed > 0) {
        speedText = (t.unit == "B") ? (_formatSize((u64)t.speed) + "/s") : (String::fromNumber((long long)t.speed) + " " + t.unit + "/s");
    } else {
        speedText = "--/s";
    }

    printf("  %s %s | %s/%s | %s | %s\n",
           bar.c_str(),
           Bold(pctText).c_str(),
           t.current.c_str(), t.total.c_str(),
           Yellow(speedText).c_str(),
           Gray(t.eta).c_str());
  }

  int lines = (int)tasks.size();
  if (!message.isEmpty()) lines += 1;
  if (lines > 0) {
      printf("\x1b[%dA", lines);
  }
  fflush(stdout);
}

// Spinner
Spinner::~Spinner() {
  if (_running) stop();
}

void Spinner::start(const String &msg) {
  _running = true;
  _msg = msg;
  printf("\x1b[?25l");
}

void Spinner::update(const String &msg) {
  static const char *frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
  if (!msg.isEmpty()) _msg = msg;
  printf("\r\x1b[K%s %s ", Cyan(frames[_frame % 10]).c_str(), _msg.c_str());
  fflush(stdout);
  _frame++;
}

void Spinner::stop(const String &finalMsg, bool success) {
  if (!_running) return;
  _running = false;
  String icon = success ? Green("✔") : Red("✖");
  printf("\r\x1b[K%s %s\n", icon.c_str(), finalMsg.isEmpty() ? _msg.c_str() : finalMsg.c_str());
  printf("\x1b[?25h");
  fflush(stdout);
}

} // namespace Ksee

