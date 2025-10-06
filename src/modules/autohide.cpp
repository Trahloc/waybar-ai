#include "modules/autohide.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace waybar::modules {

/**
 * @brief Initialize Autohide module: configure thresholds/delays, register IPC events, and start
 * mouse tracking.
 *
 * Constructs an Autohide instance for the given bar using the provided configuration. Reads
 * threshold and timing options from `config` (falling back to sensible defaults), marks Hyprland
 * modules as ready for IPC, registers for `workspacev2` and `focusedmonv2` events, and launches the
 * background mouse-tracking thread that drives autohide behavior.
 *
 * @param id Module identifier.
 * @param bar Reference to the Bar this module controls.
 * @param config JSON configuration used to set thresholds, delays, and the mouse check interval.
 */
Autohide::Autohide(const std::string& id, const Bar& bar, const Json::Value& config)
    : AModule(config, "autohide", id, false, false),
      config_(config),
      bar_(const_cast<Bar*>(&bar)),
      m_ipc(waybar::modules::hyprland::IPC::inst()),
      waybar_state_(WaybarState::VISIBLE),  // Start with waybar visible (as it is by default)
      mouse_thread_running_(false),
      mouse_thread_exit_(false) {
  // Set modulesReady flag - this is required for IPC to work
  // This is safe because all Hyprland modules do this
  waybar::modules::hyprland::modulesReady = true;

  // Get configuration values
  threshold_hidden_y_ =
      config_["threshold-hidden-y"].isUInt() ? config_["threshold-hidden-y"].asUInt() : 1;
  threshold_visible_y_ =
      config_["threshold-visible-y"].isUInt() ? config_["threshold-visible-y"].asUInt() : 50;
  delay_show_ = config_["delay-show"].isUInt() ? config_["delay-show"].asUInt() : 0;
  delay_hide_ = config_["delay-hide"].isUInt() ? config_["delay-hide"].asUInt() : 3000;
  check_interval_ = config_["check-interval"].isUInt() ? config_["check-interval"].asUInt() : 100;

  spdlog::info(
      "Autohide module initialized - hidden_y: {}, visible_y: {}, delay_show: {}ms, delay_hide: "
      "{}ms, interval: {}ms",
      threshold_hidden_y_, threshold_visible_y_, delay_show_, delay_hide_, check_interval_);

  // Register for workspace events - the IPC system will handle the registration
  // even if it's not ready yet (it will queue the registration)
  spdlog::info("Autohide: Registering for workspace events");
  m_ipc.registerForIPC("workspacev2", this);
  m_ipc.registerForIPC("focusedmonv2", this);

  // dp.emit() will automatically call update() on the main thread

  // Start mouse tracking thread
  startMouseTracking();
}

/**
 * @brief Cleanly shuts down autohide by stopping the background mouse-tracking and unregistering
 * from IPC.
 *
 * Ensures the mouse-tracking thread is stopped before removing IPC callbacks to avoid races with
 * incoming events.
 */
Autohide::~Autohide() {
  stopMouseTracking();
  m_ipc.unregisterForIPC(this);
}

/**
 * @brief Start background mouse-tracking for autohide behavior.
 *
 * If tracking is already running this is a no-op. Otherwise this resets the exit
 * flag, spawns the background thread that polls the cursor position, and marks
 * tracking as running.
 */
void Autohide::startMouseTracking() {
  if (mouse_thread_running_.load()) {
    return;
  }

  spdlog::debug("Autohide: Starting mouse tracking thread");
  mouse_thread_exit_.store(false);
  mouse_thread_ = std::thread(&Autohide::mouseTrackingThread, this);
  mouse_thread_running_.store(true);
}

/**
 * @brief Stops the background mouse-tracking thread and waits for it to exit.
 *
 * If tracking is not running this is a no-op. Signals the tracking thread to exit,
 * joins it if joinable, and updates the internal running flag.
 */
void Autohide::stopMouseTracking() {
  if (!mouse_thread_running_.load()) {
    return;
  }

  spdlog::debug("Autohide: Stopping mouse tracking thread");
  mouse_thread_exit_.store(true);

  if (mouse_thread_.joinable()) {
    mouse_thread_.join();
  }
  mouse_thread_running_.store(false);
}

/**
 * @brief Background thread entry that periodically polls the mouse position.
 *
 * Runs until the internal exit flag is set, invoking checkMousePosition() at
 * regular intervals determined by `check_interval_`.
 */
void Autohide::mouseTrackingThread() {
  spdlog::debug("Autohide: Mouse tracking thread started");

  while (!mouse_thread_exit_.load()) {
    if (bar_) {
      checkMousePosition();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_));
  }

  spdlog::debug("Autohide: Mouse tracking thread stopped");
}

/**
 * @brief Update autohide state based on the cursor position on the bar's monitor.
 *
 * Checks the current cursor position and, if the cursor is on the same monitor as the bar,
 * converts it to monitor-relative coordinates and updates the module's autohide state.
 * - A cursor at or above `threshold_hidden_y_` is treated as a top "show" trigger (requires
 *   two consecutive top triggers to schedule a show).
 * - A cursor below `threshold_visible_y_` is treated as a "hide" trigger and schedules a hide.
 * Pending transitions are timed using `delay_show_` / `delay_hide_` (minimum 10 ms); when a
 * pending transition elapses the state becomes `VISIBLE` or `HIDDEN` and `dp.emit()` is invoked.
 *
 * If the cursor position cannot be obtained or the cursor is not on the bar's monitor, no state
 * changes or events are performed.
 */
void Autohide::checkMousePosition() {
  int mouse_x, mouse_y;

  if (!getMousePosition(mouse_x, mouse_y)) {
    spdlog::debug("Autohide: Failed to get mouse position");
    return;  // Failed to get mouse position
  }

  // Get monitor geometry to check if mouse is on this monitor
  if (!bar_ || !bar_->output || !bar_->output->monitor) {
    spdlog::debug("Autohide: No bar, output, or monitor available");
    return;
  }

  auto monitor_geometry = *bar_->output->monitor->property_geometry().get_value().gobj();

  // Check if mouse is actually on this monitor
  if (mouse_x < monitor_geometry.x || mouse_x >= monitor_geometry.x + monitor_geometry.width ||
      mouse_y < monitor_geometry.y || mouse_y >= monitor_geometry.y + monitor_geometry.height) {
    spdlog::debug("Autohide: Mouse at ({},{}) not on monitor (geometry: x={}, y={}, w={}, h={})",
                  mouse_x, mouse_y, monitor_geometry.x, monitor_geometry.y, monitor_geometry.width,
                  monitor_geometry.height);
    return;  // Mouse is not on this monitor, ignore
  }

  // Convert to monitor-relative coordinates
  int monitor_mouse_y = mouse_y - monitor_geometry.y;

  // Log mouse position changes (trace level to avoid spam)
  spdlog::trace("Autohide: Mouse at screen ({},{}) -> monitor y={}, state={}", mouse_x, mouse_y,
                monitor_mouse_y, static_cast<int>(waybar_state_.load()));

  // Simple logic: mouse_y <= 1px = visible, mouse_y > 50px = hidden
  if (monitor_mouse_y <= static_cast<int>(threshold_hidden_y_)) {
    // Mouse at top 1px - should show waybar (requires two consecutive events)
    if (last_trigger_was_show_) {
      // This is the second consecutive show trigger
      if (waybar_state_ == WaybarState::HIDDEN) {
        spdlog::debug(
            "Autohide: Mouse at y={} (<=1px) on monitor - second consecutive trigger, "
            "scheduling show",
            monitor_mouse_y);
        waybar_state_ = WaybarState::PENDING_VISIBLE;
        timer_start_ = std::chrono::steady_clock::now();
      } else if (waybar_state_ == WaybarState::PENDING_HIDDEN) {
        spdlog::debug(
            "Autohide: Mouse at y={} (<=1px) on monitor - second consecutive trigger, canceling "
            "hide, scheduling show",
            monitor_mouse_y);
        waybar_state_ = WaybarState::PENDING_VISIBLE;
        timer_start_ = std::chrono::steady_clock::now();
      }
    } else {
      // First show trigger - mark it but don't act yet
      spdlog::trace(
          "Autohide: Mouse at y={} (<=1px) on monitor - first show trigger, waiting for second",
          monitor_mouse_y);
    }
    last_trigger_was_show_ = true;
  } else if (monitor_mouse_y > static_cast<int>(threshold_visible_y_)) {
    // Mouse below 50px - should hide waybar (only if currently visible)
    if (waybar_state_ == WaybarState::VISIBLE) {
      spdlog::trace("Autohide: Mouse at y={} (>50px) on monitor - scheduling hide",
                    monitor_mouse_y);
      waybar_state_ = WaybarState::PENDING_HIDDEN;
      timer_start_ = std::chrono::steady_clock::now();
    } else if (waybar_state_ == WaybarState::PENDING_VISIBLE) {
      spdlog::trace("Autohide: Mouse at y={} (>50px) on monitor - canceling show, scheduling hide",
                    monitor_mouse_y);
      waybar_state_ = WaybarState::PENDING_HIDDEN;
      timer_start_ = std::chrono::steady_clock::now();
    }
    // If already PENDING_HIDDEN, don't reset the timer - let it continue counting
    // This ensures the timer only starts once when entering the hide zone

    // Reset the show trigger flag when mouse moves to hide zone
    last_trigger_was_show_ = false;
  } else {
    // Mouse is between 1px and 50px - reset the show trigger flag
    last_trigger_was_show_ = false;
  }

  // Check if pending actions should execute
  auto now = std::chrono::steady_clock::now();
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - timer_start_).count();

  if (waybar_state_ == WaybarState::PENDING_VISIBLE) {
    // Use minimum 10ms delay to avoid race conditions and timing issues
    uint32_t effective_delay = std::max(delay_show_, 10u);
    if (elapsed >= static_cast<long>(effective_delay)) {
      spdlog::debug("Autohide: Executing delayed show after {}ms", elapsed);
      waybar_state_ = WaybarState::VISIBLE;
      dp.emit();
    }
  } else if (waybar_state_ == WaybarState::PENDING_HIDDEN) {
    // Use minimum 10ms delay to avoid race conditions and timing issues
    uint32_t effective_delay = std::max(delay_hide_, 10u);
    if (elapsed >= static_cast<long>(effective_delay)) {
      spdlog::debug("Autohide: Executing delayed hide after {}ms", elapsed);
      waybar_state_ = WaybarState::HIDDEN;
      dp.emit();
    }
  }
}

/**
 * @brief Retrieves the global cursor position from Hyprland IPC.
 *
 * Queries the Hyprland IPC "cursorpos" endpoint and parses the response into
 * integer screen coordinates.
 *
 * @param[out] x Global X coordinate of the cursor on success.
 * @param[out] y Global Y coordinate of the cursor on success.
 * @return true if the cursor position was successfully obtained and parsed;
 *         false otherwise.
 */
bool Autohide::getMousePosition(int& x, int& y) {
  // Use Hyprland IPC to get global cursor position
  try {
    auto reply = m_ipc.getSocket1Reply("cursorpos");
    if (reply.empty()) {
      return false;
    }

    // Parse the reply format: "x,y" (e.g., "1234,567")
    size_t comma_pos = reply.find(',');
    if (comma_pos == std::string::npos) {
      return false;
    }

    x = std::stoi(reply.substr(0, comma_pos));
    y = std::stoi(reply.substr(comma_pos + 1));
    return true;
  } catch (const std::exception& e) {
    spdlog::debug("Autohide: Failed to get cursor position via IPC: {}", e.what());
    return false;
  }
}

/**
 * @brief Apply the current autohide state to the associated bar's visibility.
 *
 * Sets the bar's mode to Bar::MODE_DEFAULT when the state is VISIBLE or PENDING_HIDDEN,
 * and to Bar::MODE_INVISIBLE when the state is HIDDEN or PENDING_VISIBLE.
 *
 * @note This method runs on the main thread and is safe to perform GTK operations.
 */
void Autohide::update() {
  // This method runs on the main thread, so it's safe to call GTK operations
  if (!bar_) {
    spdlog::warn("Autohide: Bar pointer is null, cannot update visibility");
    return;
  }

  switch (waybar_state_.load()) {
    case WaybarState::VISIBLE:
    case WaybarState::PENDING_HIDDEN:
      bar_->setMode(Bar::MODE_DEFAULT);
      break;
    case WaybarState::HIDDEN:
    case WaybarState::PENDING_VISIBLE:
      bar_->setMode(Bar::MODE_INVISIBLE);
      break;
  }
}

/**
 * @brief Handle IPC events and force the bar visible on workspace or monitor changes.
 *
 * When the event name is "workspacev2" or "focusedmonv2", sets the autohide state to VISIBLE
 * and emits the dispatcher so update() runs on the main thread.
 *
 * @param ev Raw event string; the event name is taken as the substring before the first '>' if
 * present.
 */
void Autohide::onEvent(const std::string& ev) {
  std::string eventName;
  size_t pos = ev.find_first_of('>');
  if (pos == std::string::npos) {
    eventName = ev;
  } else {
    eventName = ev.substr(0, pos);
  }

  if (eventName == "workspacev2" || eventName == "focusedmonv2") {
    spdlog::trace("Autohide: Workspace/monitor changed - forcing waybar visible");

    // Force waybar to be visible when workspace changes
    waybar_state_ = WaybarState::VISIBLE;
    dp.emit();  // Trigger update() on main thread
  }
}

}  // namespace waybar::modules