#include "modules/cava/cava.hpp"

#include <spdlog/spdlog.h>

/**
 * @brief Create and initialize a Cava module instance with the given identifier and configuration.
 *
 * Initializes base ALabel state and obtains the shared CavaBackend instance, reads configuration
 * options (notably `hide_on_silence` and `format_silent` when present), retrieves the backend's
 * ASCII range, connects backend update and silence signals to the module handlers, and requests
 * an initial backend update to populate the module state.
 *
 * @param id Module identifier used for labeling and styling.
 * @param config JSON configuration object from which module options are read.
 */
waybar::modules::cava::Cava::Cava(const std::string& id, const Json::Value& config)
    : ALabel(config, "cava", id, "{}", 60, false, false, false),
      backend_{waybar::modules::cava::CavaBackend::inst(config)} {
  if (config_["hide_on_silence"].isBool()) hide_on_silence_ = config_["hide_on_silence"].asBool();
  if (config_["format_silent"].isString()) format_silent_ = config_["format_silent"].asString();

  ascii_range_ = backend_->getAsciiRange();
  backend_->signal_update().connect(sigc::mem_fun(*this, &Cava::onUpdate));
  backend_->signal_silence().connect(sigc::mem_fun(*this, &Cava::onSilence));
  backend_->Update();
}

/**
 * @brief Dispatches a named action to this module's action handler.
 *
 * Invokes the member action associated with the given name if present; logs an error if no matching action exists.
 *
 * @param name Action identifier used to select and invoke the corresponding module action.
 */
auto waybar::modules::cava::Cava::doAction(const std::string& name) -> void {
  if ((actionMap_[name])) {
    (this->*actionMap_[name])();
  } else
    spdlog::error("Cava. Unsupported action \"{0}\"", name);
}

/**
 * @brief Toggle the Cava backend's playback state between paused and resumed.
 *
 * Calls the backend to switch its current pause/resume state.
 */
void waybar::modules::cava::Cava::pause_resume() { backend_->doPauseResume(); }
/**
 * @brief Update the module's label from backend audio data and apply updated styling.
 *
 * Clears and repopulates the label markup using icons derived from each character in
 * `input`, makes the label visible, triggers a visual update, and marks the module as not silent.
 *
 * @param input String of characters produced by the backend where each character encodes an ASCII-based level; characters above `ascii_range_` are clamped to `ascii_range_`.
 */
auto waybar::modules::cava::Cava::onUpdate(const std::string& input) -> void {
  if (silence_) {
    label_.get_style_context()->remove_class("silent");
    label_.get_style_context()->add_class("updated");
  }
  label_text_.clear();
  for (auto& ch : input)
    label_text_.append(getIcon((ch > ascii_range_) ? ascii_range_ : ch, "", ascii_range_ + 1));

  label_.set_markup(label_text_);
  label_.show();
  ALabel::update();
  silence_ = false;
}
/**
 * @brief Switches the module to its silent state and updates the label presentation.
 *
 * When the module is not already marked silent, this updates the label's style by
 * removing the "updated" class, then either hides the label (if configured to do so)
 * or applies the configured silent markup. Finally, marks the module as silent and
 * adds the "silent" style class to the label.
 */
auto waybar::modules::cava::Cava::onSilence() -> void {
  if (!silence_) {
    label_.get_style_context()->remove_class("updated");

    if (hide_on_silence_)
      label_.hide();
    else if (config_["format_silent"].isString())
      label_.set_markup(format_silent_);
    silence_ = true;
    label_.get_style_context()->add_class("silent");
  }
}