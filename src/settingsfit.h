#pragma once

#include <functional>

class QComboBox;
class QTabWidget;
class QWidget;

// This edition's finishing of the team's Settings layout, so nothing on it is cut off in any language
// (tests/test_settingsfit.cpp). Kept apart from settingsdialog.cpp, which the team changes often.
namespace SettingsFit {

// Rebuilds a 'responsive_' container's layout with its columns side by side or one under the other
// (SettingsDialog::switchContainerLayout, the team's).
using SwitchLayout = std::function<void(QWidget *container, bool useHorizontal)>;

// Every dropdown as wide as its longest entry, every number field as wide as its largest number.
void fitControls(QWidget *window);
void fitComboBoxToEntries(QComboBox *comboBox);

// Hide a setting's row: the control and the label in front of it, with no gap left in the form.
void hideRow(QWidget *field);

// Two columns side by side where they fit, one under the other where they do not, for every tab.
void updateColumns(QWidget *window, const SwitchLayout &switchLayout);

// On the first show: the window's size and place (see settingsfit.cpp).
void fitWindow(QWidget *window, QTabWidget *tabs, const SwitchLayout &switchLayout);

} // namespace SettingsFit
