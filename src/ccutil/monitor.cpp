
#include <tesseract/preparation.h> // compiler config, etc.

#include <tesseract/ocrclass.h>

#include "rect.h"

#include <cmath>

namespace tesseract {

ETEXT_DESC::ETEXT_DESC() {
  end_time = std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>();
}

// Sets the end time to be deadline_msecs milliseconds from now.
void ETEXT_DESC::set_deadline_msecs(int32_t deadline_msecs) {
  if (deadline_msecs > 0) {
    end_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(deadline_msecs);
  } else {
    end_time = std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>();
  }
}

// Returns false if we've not passed the end_time, or have not set a deadline.
bool ETEXT_DESC::deadline_exceeded() const {
  if (end_time.time_since_epoch() <= std::chrono::steady_clock::duration::zero()) {
    return false;
  }
  auto now = std::chrono::steady_clock::now();
  return (now > end_time);
}

// Return true when cancel state has been flagged through whatever means.
bool ETEXT_DESC::kick_watchdog_and_check_for_cancel(int word_count) {
  ocr_alive = true;
  if (abort_the_action)
    return true;
  if (deadline_exceeded()) {
    // abort_the_action = true;   -- don't set this flag here as we do not want the deadline signal to "stick": the cancel call can reset the deadline, resulting in temporary cancel/interuption, by design. When userland code wishes to completely abort the action, it can set the `abort_the_action` flag itself.
    return true;
  }
  if (cancel != nullptr && (*cancel)(this, word_count)) {
    // abort_the_action = true;;   -- don't set this flag here as we do not want the deadline signal to "stick": the cancel call can reset the deadline, resulting in temporary cancel/interuption, by design. When userland code wishes to completely abort the action, it can set the `abort_the_action` flag itself.
    return true;
  }
  return false;
}

ETEXT_DESC& ETEXT_DESC::bump_progress(int part_count, int whole_count) noexcept {
  if (whole_count <= 1 || part_count < 1 || part_count >= whole_count || progress >= 70)
    return bump_progress();

  float rate = part_count * 20.0 / whole_count;
  progress += rate;

  return *this;
}

ETEXT_DESC& ETEXT_DESC::set_progress(float percentage) noexcept {
  percentage = std::min(100.0f, std::max(0.0f, percentage));
  progress = percentage;

  return *this;
}

ETEXT_DESC &ETEXT_DESC::bump_progress() noexcept {
  if (progress < 25)
    progress += 0.1;
  else if (progress < 85)
    progress += 0.01;
  else if (progress < 99)
    progress += 0.001;
  else if (progress < 99.5)
    progress += 0.0001;
  // else: stop incrementing progress
  return *this;
}

ETEXT_DESC &ETEXT_DESC::exec_progress_func(int left, int right, int top, int bottom) {
  // don't hammer the userland progress callback; only call it when there's "significant" progress...
  if (std::isnan(previous_progress) || fabs(progress - previous_progress) >= 0.1) {
    if (progress_callback != nullptr) {
      (*progress_callback)(this, left, right, top, bottom);
    }
    //      v---------- the progress callback is supposed to do this itself, so it can fully control when it will be re-invoked.
    //previous_progress = progress;
  }
  return *this;
}

ETEXT_DESC &ETEXT_DESC::exec_progress_func(const TBOX *box) {
  if (box != nullptr)
    return exec_progress_func(*box);
  else
    return exec_progress_func();
}

ETEXT_DESC &ETEXT_DESC::exec_progress_func(const TBOX &box) {
  return exec_progress_func(box.left(), box.right(), box.top(), box.bottom());
}

ETEXT_DESC &ETEXT_DESC::exec_progress_func() {
  return exec_progress_func(-1, -1, -1, -1);
}

void ETEXT_DESC::reset_values_to_Factory_Defaults() {
  end_time = std::chrono::time_point<std::chrono::steady_clock, std::chrono::milliseconds>();
  ocr_alive = true;
  progress = 0.0;
  previous_progress = NAN;
  abort_the_action = false;
}

} // namespace tesseract
