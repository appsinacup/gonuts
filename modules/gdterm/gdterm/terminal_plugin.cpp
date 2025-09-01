#ifdef TOOLS_ENABLED

#include "terminal_plugin.h"
#include "core/input/shortcut.h"
#include "core/io/file_access.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/scroll_bar.h"
#include "scene/gui/tab_container.h"
#include "servers/display_server.h"

// Singleton definition
TerminalPlugin *TerminalPlugin::singleton = nullptr;

void TerminalPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			for (int i = 0; i < terminal_tabs.size(); i++) {
				if (terminal_tabs[i].terminal && !terminal_tabs[i].terminal->is_active()) {
					terminal_tabs[i].terminal->call_deferred("start");
				}
			}
			_update_terminal_theme();
			if (active_tab >= 0) {
				_update_scrollbar(active_tab);
			}
		} break;
		case Control::NOTIFICATION_THEME_CHANGED: {
			_update_terminal_theme();
		} break;
	}
}

void TerminalPlugin::_update_terminal_theme() {
	if (!EditorNode::get_singleton()) {
		return;
	}

	Ref<Theme> editor_theme = EditorNode::get_singleton()->get_editor_theme();
	if (!editor_theme.is_valid()) {
		return;
	}

	for (int i = 0; i < terminal_tabs.size(); i++) {
		GDTerm *current_terminal = terminal_tabs[i].terminal;
		if (!current_terminal) {
			continue;
		}

		current_terminal->set_font(editor_theme->get_font(SNAME("source"), SNAME("EditorFonts")));
		current_terminal->set_bold_font(editor_theme->get_font(SNAME("bold"), SNAME("EditorFonts")));
		current_terminal->set_dim_font(editor_theme->get_font(SNAME("source"), SNAME("EditorFonts")));
		current_terminal->set_font_size(editor_theme->get_font_size(SNAME("mono_font_size"), SNAME("RichTextLabel")));
		current_terminal->set_background(editor_theme->get_color(SNAME("dark_color_1"), EditorStringName(Editor)));
		current_terminal->set_foreground(Color(0.875, 0.875, 0.875));
	}
}

void TerminalPlugin::_update_scrollbar(int tab_index) {
	if (tab_index < 0 || tab_index >= terminal_tabs.size()) {
		return;
	}

	TerminalTab &tab = terminal_tabs.write[tab_index];
	if (!tab.terminal || !tab.scrollbar) {
		return;
	}

	int num_scrollback = tab.terminal->get_num_scrollback_lines();
	int num_screen = tab.terminal->get_num_screen_lines();
	int scroll_pos = tab.terminal->get_scroll_pos();

	if (num_scrollback == 0) {
		tab.scrollbar->set_visible(false);
		return;
	}

	tab.scrollbar->set_visible(true);
	tab.scrollbar_changing = true;
	tab.scrollbar->set_min(0);
	int page_size = MAX(1, MIN(num_screen, num_scrollback));
	tab.scrollbar->set_max(num_scrollback + page_size);
	tab.scrollbar->set_page(page_size);
	tab.scrollbar->set_step(1);
	tab.scrollbar->set_value(CLAMP(scroll_pos, 0, num_scrollback));
	tab.scrollbar_changing = false;
}

void TerminalPlugin::_update_size_label() {
	if (active_tab < 0 || active_tab >= terminal_tabs.size() || !size_label) {
		return;
	}

	TerminalTab &tab = terminal_tabs.write[active_tab];
	if (!tab.terminal) {
		return;
	}

	int cols = tab.terminal->get_cols();
	int rows = tab.terminal->get_rows();
	size_label->set_text("Terminal: " + String::num(cols) + "x" + String::num(rows));
}

void TerminalPlugin::_on_scrollback_changed(int tab_index) {
	if (tab_index < 0 || tab_index >= terminal_tabs.size()) {
		return;
	}

	TerminalTab &tab = terminal_tabs.write[tab_index];
	if (!tab.scrollbar_changing) {
		_update_scrollbar(tab_index);
	}
}

void TerminalPlugin::_on_scrollbar_value_changed(double p_value, int tab_index) {
	if (tab_index < 0 || tab_index >= terminal_tabs.size()) {
		return;
	}

	TerminalTab &tab = terminal_tabs.write[tab_index];
	if (!tab.scrollbar_changing && tab.terminal) {
		tab.scrollbar_changing = true;
		int row = Math::round(p_value);
		int max_scroll = tab.terminal->get_num_scrollback_lines();
		row = CLAMP(row, 0, max_scroll);
		tab.terminal->set_scroll_pos(row);
		tab.scrollbar_changing = false;
	}
}

void TerminalPlugin::_on_tab_changed(int p_tab) {
	active_tab = p_tab;
	if (active_tab >= 0 && active_tab < terminal_tabs.size()) {
		_update_scrollbar(active_tab);
		_update_size_label();
		TerminalTab &tab = terminal_tabs.write[active_tab];
		if (tab.terminal && tab.terminal->is_inside_tree()) {
			tab.terminal->grab_focus();
		}
	}
}

void TerminalPlugin::_on_add_terminal_pressed() {
	int selected_type = terminal_type_option->get_selected();
	if (selected_type < 0 || selected_type >= available_terminals.size()) {
		return;
	}

	TerminalType &term_type = available_terminals.write[selected_type];

	String terminal_name = term_type.display_name;

	_add_terminal_tab(terminal_name, term_type);
}

void TerminalPlugin::_on_tab_close_pressed(int p_tab) {
	_close_terminal_tab(p_tab);
}

void TerminalPlugin::_detect_available_terminals() {
	available_terminals.clear();

	// Most popular shells per platform
	const char *shells[] = {
		// macOS/Linux
		"zsh", "/bin/zsh",
		"bash", "/opt/homebrew/bin/bash",
		"bash", "/bin/bash",
		"pwsh", "/opt/homebrew/bin/pwsh", // PowerShell Core
		"pwsh", "/usr/local/bin/pwsh",
		// Windows (when supported)
		"cmd", "C:\\Windows\\System32\\cmd.exe",
		"powershell", "C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe"
	};

	Vector<String> added_names;

	for (size_t i = 0; i < sizeof(shells) / sizeof(shells[0]); i += 2) {
		String shell_name = shells[i];
		String shell_path = shells[i + 1];

		if (FileAccess::exists(shell_path) && !added_names.has(shell_name)) {
			TerminalType term_type;
			term_type.name = shell_name;
			term_type.path = shell_path;
			term_type.display_name = shell_name;
			available_terminals.push_back(term_type);
			added_names.push_back(shell_name);
		}
	}

	// Popular interpreters
	const char *interpreters[] = {
		"python3", "/usr/bin/python3",
		"python3", "/opt/homebrew/bin/python3",
		"node", "/opt/homebrew/bin/node",
		"node", "/usr/local/bin/node"
	};

	for (size_t i = 0; i < sizeof(interpreters) / sizeof(interpreters[0]); i += 2) {
		String interp_name = interpreters[i];
		String interp_path = interpreters[i + 1];

		if (FileAccess::exists(interp_path) && !added_names.has(interp_name)) {
			TerminalType term_type;
			term_type.name = interp_name;
			term_type.path = interp_path;
			term_type.display_name = interp_name;
			available_terminals.push_back(term_type);
			added_names.push_back(interp_name);
		}
	}

	if (available_terminals.is_empty()) {
		TerminalType term_type;
		term_type.name = "sh";
		term_type.path = "/bin/sh";
		term_type.display_name = "sh";
		available_terminals.push_back(term_type);
	}
}

void TerminalPlugin::_close_terminal_tab(int tab_index) {
	if (tab_index < 0 || tab_index >= terminal_tabs.size()) {
		return;
	}

	if (terminal_tabs.size() <= 1) {
		return;
	}

	TerminalTab &tab = terminal_tabs.write[tab_index];

	if (tab.terminal) {
		tab.terminal->queue_free();
	}
	if (tab.terminal_hbox) {
		tab_container->remove_child(tab.terminal_hbox);
		tab.terminal_hbox->queue_free();
	}

	terminal_tabs.remove_at(tab_index);

	if (active_tab >= terminal_tabs.size()) {
		active_tab = terminal_tabs.size() - 1;
	}
	if (active_tab >= 0) {
		tab_container->set_current_tab(active_tab);
		_update_scrollbar(active_tab);
	}
}

void TerminalPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_scrollback_changed", "tab_index"), &TerminalPlugin::_on_scrollback_changed);
	ClassDB::bind_method(D_METHOD("_on_scrollbar_value_changed", "value", "tab_index"), &TerminalPlugin::_on_scrollbar_value_changed);
	ClassDB::bind_method(D_METHOD("_on_tab_changed", "tab"), &TerminalPlugin::_on_tab_changed);
	ClassDB::bind_method(D_METHOD("_on_add_terminal_pressed"), &TerminalPlugin::_on_add_terminal_pressed);
	ClassDB::bind_method(D_METHOD("_on_tab_close_pressed", "tab"), &TerminalPlugin::_on_tab_close_pressed);
	ClassDB::bind_method(D_METHOD("_update_terminal_theme"), &TerminalPlugin::_update_terminal_theme);
	ClassDB::bind_method(D_METHOD("_update_size_label"), &TerminalPlugin::_update_size_label);

	// Public API methods
	ClassDB::bind_method(D_METHOD("add_terminal_tab", "name", "shell_path"), &TerminalPlugin::add_terminal_tab, DEFVAL("Terminal"), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("run_command_in_tab", "tab_index", "command"), &TerminalPlugin::run_command_in_tab);
	ClassDB::bind_method(D_METHOD("run_command_in_current_tab", "command"), &TerminalPlugin::run_command_in_current_tab);
	ClassDB::bind_method(D_METHOD("get_current_tab_index"), &TerminalPlugin::get_current_tab_index);
	ClassDB::bind_method(D_METHOD("get_tab_count"), &TerminalPlugin::get_tab_count);
	ClassDB::bind_method(D_METHOD("get_tab_name", "tab_index"), &TerminalPlugin::get_tab_name);
	ClassDB::bind_method(D_METHOD("set_current_tab", "tab_index"), &TerminalPlugin::set_current_tab);
	ClassDB::bind_method(D_METHOD("close_tab", "tab_index"), &TerminalPlugin::close_tab);
}

void TerminalPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		EditorNode::get_bottom_panel()->make_item_visible(container);
	}
}

void TerminalPlugin::_create_ui() {
	_detect_available_terminals();

	container = memnew(MarginContainer);
	container->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	container->set_v_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);

	main_vbox = memnew(VBoxContainer);
	main_vbox->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	main_vbox->set_v_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);

	_create_toolbar();

	tab_container = memnew(TabContainer);
	tab_container->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	tab_container->set_v_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	tab_container->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	tab_container->connect("tab_changed", callable_mp(this, &TerminalPlugin::_on_tab_changed));
	tab_container->connect("gui_input", callable_mp(this, &TerminalPlugin::_gui_input));

	TabBar *tab_bar = tab_container->get_tab_bar();
	if (tab_bar) {
		tab_bar->set_tab_close_display_policy(TabBar::CLOSE_BUTTON_SHOW_ACTIVE_ONLY);
		tab_bar->connect("tab_close_pressed", callable_mp(this, &TerminalPlugin::_on_tab_close_pressed));
	}

	main_vbox->add_child(toolbar_hbox);
	main_vbox->add_child(tab_container);
	container->add_child(main_vbox);

	// Only add to editor if we're in editor context (not in doctool)
	if (EditorNode::get_singleton() && EditorNode::get_bottom_panel()) {
		EditorNode::get_bottom_panel()->add_item("Terminal", container);
	}
}

void TerminalPlugin::_create_toolbar() {
	toolbar_hbox = memnew(HBoxContainer);
	toolbar_hbox->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);

	terminal_type_option = memnew(OptionButton);

	for (int i = 0; i < available_terminals.size(); i++) {
		terminal_type_option->add_item(available_terminals[i].display_name);
	}
	terminal_type_option->select(0);

	add_terminal_button = memnew(Button);
	add_terminal_button->set_text("Create +");
	add_terminal_button->set_tooltip_text("Add Terminal");
	add_terminal_button->connect("pressed", callable_mp(this, &TerminalPlugin::_on_add_terminal_pressed));

	add_terminal_button->set_custom_minimum_size(Vector2(70, 28));
	add_terminal_button->set_h_size_flags(Control::SizeFlags::SIZE_SHRINK_CENTER);
	add_terminal_button->set_v_size_flags(Control::SizeFlags::SIZE_SHRINK_CENTER);

	size_label = memnew(Label);
	size_label->set_text("Terminal: --x--");
	size_label->set_h_size_flags(Control::SizeFlags::SIZE_SHRINK_CENTER);
	size_label->set_v_size_flags(Control::SizeFlags::SIZE_SHRINK_CENTER);

	toolbar_hbox->add_child(terminal_type_option);
	toolbar_hbox->add_child(add_terminal_button);
	toolbar_hbox->add_child(size_label);
}

void TerminalPlugin::_add_terminal_tab(const String &p_name, const TerminalType &p_type) {
	TerminalTab tab;
	tab.name = p_name;
	tab.terminal_type = p_type.name;

	tab.terminal_hbox = memnew(HBoxContainer);
	tab.terminal_hbox->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	tab.terminal_hbox->set_v_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);

	tab.terminal = memnew(GDTerm);
	tab.terminal->set_h_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	tab.terminal->set_v_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);
	tab.terminal->set_focus_mode(Control::FOCUS_ALL);
	tab.terminal->set_mouse_filter(Control::MOUSE_FILTER_STOP);

	tab.scrollbar = memnew(VScrollBar);
	tab.scrollbar->set_h_size_flags(Control::SizeFlags::SIZE_SHRINK_END);
	tab.scrollbar->set_v_size_flags(Control::SizeFlags::SIZE_EXPAND_FILL);

	int tab_index = terminal_tabs.size();
	tab.terminal->connect("scrollback_changed", callable_mp(this, &TerminalPlugin::_on_scrollback_changed).bind(tab_index));
	tab.terminal->connect("size_changed", callable_mp(this, &TerminalPlugin::_update_size_label));
	tab.scrollbar->connect("value_changed", callable_mp(this, &TerminalPlugin::_on_scrollbar_value_changed).bind(tab_index));

	tab.terminal->connect("copy_request", callable_mp(this, &TerminalPlugin::_on_copy_request).bind(tab_index));
	tab.terminal->connect("paste_request", callable_mp(this, &TerminalPlugin::_on_paste_request).bind(tab_index));

	tab.terminal_hbox->add_child(tab.terminal);
	tab.terminal_hbox->add_child(tab.scrollbar);

	tab_container->add_child(tab.terminal_hbox);
	tab_container->set_tab_title(tab_index, p_name);

	terminal_tabs.push_back(tab);

	tab_container->set_current_tab(tab_index);
	active_tab = tab_index;

	_update_terminal_theme();

	if (tab.terminal) {
		tab.terminal->set_scroll_pos(tab.terminal->get_num_scrollback_lines());
	}

	_update_scrollbar(tab_index);
	_update_size_label();
	if (tab.terminal && !tab.terminal->is_active()) {
		if (p_type.path.is_empty()) {
			tab.terminal->call_deferred("start");
		} else {
			tab.terminal->call_deferred("start_with_shell", p_type.path);
		}
		call_deferred("_update_size_label");
	}
}

TerminalPlugin::TerminalPlugin() {
	singleton = this;
	_create_ui();

	if (!available_terminals.is_empty()) {
		String default_name = available_terminals[0].display_name;
		_add_terminal_tab(default_name, available_terminals[0]);
	}
}

void TerminalPlugin::_on_copy_request(int tab_index) {
	if (tab_index < 0 || tab_index >= terminal_tabs.size()) {
		return;
	}

	TerminalTab &tab = terminal_tabs.write[tab_index];
	if (!tab.terminal) {
		return;
	}

	String selected_text = tab.terminal->get_selected_text();
	if (!selected_text.is_empty()) {
		DisplayServer::get_singleton()->clipboard_set(selected_text);
	}
}

void TerminalPlugin::_on_paste_request(int tab_index) {
	if (tab_index < 0 || tab_index >= terminal_tabs.size()) {
		return;
	}

	TerminalTab &tab = terminal_tabs.write[tab_index];
	if (!tab.terminal) {
		return;
	}

	String clipboard_text = DisplayServer::get_singleton()->clipboard_get();
	if (!clipboard_text.is_empty()) {
		tab.terminal->send_input(clipboard_text);
	}
}

void TerminalPlugin::_gui_input(const Ref<InputEvent> &p_event) {
	if (active_tab < 0 || active_tab >= terminal_tabs.size()) {
		return;
	}

	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid()) {
		if (active_tab >= 0 && active_tab < terminal_tabs.size()) {
			TerminalTab &tab = terminal_tabs.write[active_tab];
			if (tab.terminal && tab.terminal->is_inside_tree()) {
				if (mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
					tab.terminal->grab_focus();
				}
				tab.terminal->gui_input(p_event);
			}
		}
	}

	Ref<InputEventPanGesture> pg = p_event;
	if (pg.is_valid()) {
		TerminalTab &tab = terminal_tabs.write[active_tab];
		if (tab.terminal && tab.scrollbar) {
			Vector2 delta = pg->get_delta();
			if (Math::abs(delta.y) > 0.1) {
				int current_pos = tab.terminal->get_scroll_pos();
				int scroll_amount = Math::round(delta.y * 1.0);
				tab.terminal->set_scroll_pos(current_pos + scroll_amount);
				get_viewport()->set_input_as_handled();
			}
		}
	}
}

// Public methods for tab management and command execution

int TerminalPlugin::add_terminal_tab(const String &p_name, const String &p_shell_path) {
	String tab_name = p_name.is_empty() ? "Terminal" : p_name;

	TerminalType terminal_type;
	if (!p_shell_path.is_empty()) {
		terminal_type = TerminalType(tab_name, p_shell_path);
	} else if (!available_terminals.is_empty()) {
		terminal_type = available_terminals[0];
	} else {
		// Fallback to default shell
		terminal_type = TerminalType(tab_name, "/bin/bash");
	}

	_add_terminal_tab(tab_name, terminal_type);
	return terminal_tabs.size() - 1;
}

bool TerminalPlugin::run_command_in_tab(int tab_index, const String &command) {
	if (tab_index < 0 || tab_index >= terminal_tabs.size()) {
		return false;
	}

	TerminalTab &tab = terminal_tabs.write[tab_index];
	if (!tab.terminal) {
		return false;
	}

	// Send the command followed by Enter
	tab.terminal->send_input(command + "\n");
	return true;
}

bool TerminalPlugin::run_command_in_current_tab(const String &command) {
	return run_command_in_tab(active_tab, command);
}

String TerminalPlugin::get_tab_name(int tab_index) const {
	if (tab_index < 0 || tab_index >= terminal_tabs.size()) {
		return "";
	}
	return terminal_tabs[tab_index].name;
}

bool TerminalPlugin::set_current_tab(int tab_index) {
	if (tab_index < 0 || tab_index >= terminal_tabs.size() || !tab_container) {
		return false;
	}

	tab_container->set_current_tab(tab_index);
	active_tab = tab_index;
	_update_scrollbar(tab_index);
	_update_size_label();
	return true;
}

bool TerminalPlugin::close_tab(int tab_index) {
	if (tab_index < 0 || tab_index >= terminal_tabs.size()) {
		return false;
	}

	_close_terminal_tab(tab_index);
	return true;
}

TerminalPlugin::~TerminalPlugin() {
	for (int i = 0; i < terminal_tabs.size(); i++) {
		if (terminal_tabs[i].terminal) {
			memdelete(terminal_tabs[i].terminal);
		}
		if (terminal_tabs[i].scrollbar) {
			memdelete(terminal_tabs[i].scrollbar);
		}
		if (terminal_tabs[i].terminal_hbox) {
			memdelete(terminal_tabs[i].terminal_hbox);
		}
	}

	if (container && EditorNode::get_singleton() && EditorNode::get_bottom_panel()) {
		EditorNode::get_bottom_panel()->remove_item(container);
		memdelete(container);
	}

	singleton = nullptr;
}

#endif // TOOLS_ENABLED
