#include "AIconLabel.hpp"

#include <gdkmm/pixbuf.h>
#include <spdlog/spdlog.h>

namespace waybar {

/**
 * @brief Construct an icon+label widget configured from JSON.
 *
 * Configures widget styling, orientation, spacing, and child order based on the
 * provided configuration and parameters. The constructor:
 * - Moves module/style classes from the internal label to the outer box and,
 *   if `id` is non-empty, applies `id` as a style class to the box.
 * - Reads `config["rotate"]` to determine orientation (accepted values are
 *   multiples of 90 degrees; other values are treated as 0). Rotation affects
 *   whether the box is horizontal or vertical.
 * - Reads `config["icon-spacing"]` (defaults to 8) and applies it as spacing.
 * - Handles `config["swap-icon-label"]`: accepts `null` (no-op), a boolean to
 *   swap the order of icon and label, or logs a warning and uses the default
 *   false for invalid types.
 * - Orders the icon and label according to rotation and the swap flag, then
 *   adds the composed box to the event container.
 *
 * @param config JSON configuration that may include:
 *               - `rotate` (unsigned int, degrees; multiples of 90),
 *               - `icon-spacing` (int, defaults to 8),
 *               - `swap-icon-label` (bool or null),
 *               - `icon` (bool to enable/disable icon visibility).
 * @param name   Module name assigned to the outer box widget.
 * @param id     Optional identifier used as a style class on the outer box.
 */
AIconLabel::AIconLabel(const Json::Value &config, const std::string &name, const std::string &id,
                       const std::string &format, uint16_t interval, bool ellipsize,
                       bool enable_click, bool enable_scroll)
    : ALabel(config, name, id, format, interval, ellipsize, enable_click, enable_scroll) {
  event_box_.remove();
  label_.unset_name();
  label_.get_style_context()->remove_class(MODULE_CLASS);
  box_.get_style_context()->add_class(MODULE_CLASS);
  if (!id.empty()) {
    label_.get_style_context()->remove_class(id);
    box_.get_style_context()->add_class(id);
  }

  int rot = 0;

  if (config_["rotate"].isUInt()) {
    rot = config["rotate"].asUInt() % 360;
    if ((rot % 90) != 00) rot = 0;
    rot /= 90;
  }

  if ((rot % 2) == 0)
    box_.set_orientation(Gtk::Orientation::ORIENTATION_HORIZONTAL);
  else
    box_.set_orientation(Gtk::Orientation::ORIENTATION_VERTICAL);
  box_.set_name(name);

  int spacing = config_["icon-spacing"].isInt() ? config_["icon-spacing"].asInt() : 8;
  box_.set_spacing(spacing);

  bool swap_icon_label = false;
  if (config_["swap-icon-label"].isNull()) {
  } else if (config_["swap-icon-label"].isBool()) {
    swap_icon_label = config_["swap-icon-label"].asBool();
  } else {
    spdlog::warn("'swap-icon-label' must be a bool, found '{}'. Using default value (false).",
                 config_["swap-icon-label"].asString());
  }

  if ((rot == 0 || rot == 3) ^ swap_icon_label) {
    box_.add(image_);
    box_.add(label_);
  } else {
    box_.add(label_);
    box_.add(image_);
  }

  event_box_.add(box_);
}

auto AIconLabel::update() -> void {
  image_.set_visible(image_.get_visible() && iconEnabled());
  ALabel::update();
}

bool AIconLabel::iconEnabled() const {
  return config_["icon"].isBool() ? config_["icon"].asBool() : false;
}

}  // namespace waybar