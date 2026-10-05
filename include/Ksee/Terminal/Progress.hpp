/**
 * @file Progress.hpp
 * @brief Terminal progress indicators, spinners, and multi-task progress bars for Ksee.
 */

#ifndef KSEE_TERMINAL_PROGRESS_HPP
#define KSEE_TERMINAL_PROGRESS_HPP

#include "../Types.hpp"
#include "../String.hpp"
#include "../Array.hpp"

namespace Ksee {

struct LinearTask {
    String total;
    String current;
    String eta;
    String message;
    double speed = 0.0;
    u64 totalRaw = 0;
    u64 currentRaw = 0;
    u64 startTime = 0;
    u64 lastUpdateTime = 0;
    String unit = "it";
    bool loading = false;
};

class Progress {
public:
    Array<LinearTask> tasks;
    String message;

    Progress();
    ~Progress();

    usz addLinearTask(u64 total, const String& unit = "it", const String& msg = "");
    void updateLinearTask(usz taskIdx, u64 current, const String& msg = "");
    void addLinearTaskDelta(usz taskIdx, u64 delta, const String& msg = "");
    void update();
    void destroy();

private:
    bool _started = false;
    bool _hiddenCursor = false;
    void _render();
    static String _formatSize(u64 bytes);
    static String _formatTime(double seconds);
};

class Spinner {
public:
    Spinner() = default;
    ~Spinner();

    void start(const String &msg = "");
    void update(const String &msg = "");
    void stop(const String &finalMsg = "", bool success = true);

private:
    bool _running = false;
    int _frame = 0;
    String _msg;
};

} // namespace Ksee

#endif // KSEE_TERMINAL_PROGRESS_HPP

