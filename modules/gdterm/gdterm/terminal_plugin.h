#pragma once

#ifdef TOOLS_ENABLED
#include "editor/plugins/editor_plugin.h"
#endif

#include "gdterm.h"

class MarginContainer;
class VBoxContainer;
class HBoxContainer;
class VScrollBar;
class TabContainer;
class OptionButton;
class Button;
class Label;

// Structure to represent terminal types available in the system
struct TerminalType {
	String name;
	String executable_path;
	String path; // alias for executable_path
	String display_name;
	Vector<String> args;
	String description;

	TerminalType() {}
	TerminalType(const String &p_name, const String &p_path, const Vector<String> &p_args = Vector<String>(), const String &p_desc = String()) :
			name(p_name), executable_path(p_path), path(p_path), display_name(p_name), args(p_args), description(p_desc) {}
};

// Structure to represent individual terminal tabs
struct TerminalTab {
	GDTerm *terminal;
	VScrollBar *scrollbar;
	HBoxContainer *terminal_hbox;
	String name;
	TerminalType type;
	String terminal_type;
	bool scrollbar_changing = false;

	TerminalTab() :
			terminal(nullptr), scrollbar(nullptr), terminal_hbox(nullptr) {}
};

#ifdef TOOLS_ENABLED
class TerminalPlugin : public EditorPlugin {
	GDCLASS(TerminalPlugin, EditorPlugin);

	MarginContainer *container;
	HBoxContainer *hbox;
	VScrollBar *scrollbar;
	GDTerm *terminal;
	bool scrollbar_changing = false;

	// Multi-tab support
	Vector<TerminalTab> terminal_tabs;
	int active_tab = -1;
	Vector<TerminalType> available_terminals;

	// UI components for multi-tab interface
	VBoxContainer *main_vbox;
	TabContainer *tab_container;
	OptionButton *terminal_type_option;
	HBoxContainer *toolbar_hbox;
	Button *add_terminal_button;
	Label *status_label;
	Label *size_label;

	void _on_scrollback_changed(int tab_index);
	void _on_scrollbar_value_changed(double p_value, int tab_index);
	void _on_tab_changed(int p_tab);
	void _on_add_terminal_pressed();
	void _on_tab_close_pressed(int p_tab);
	void _on_copy_request(int tab_index);
	void _on_paste_request(int tab_index);

	virtual void _gui_input(const Ref<InputEvent> &p_event);

	void _create_ui();
	void _create_toolbar();
	void _update_terminal_theme();
	void _update_scrollbar(int tab_index);
	void _update_size_label();
	void _add_terminal_tab(const String &p_name, const TerminalType &p_type);
	void _add_terminal_tab(const String &p_name, const String &p_type);
	void _detect_available_terminals();
	void _close_terminal_tab(int tab_index);
	void _update_status_label();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	virtual String get_plugin_name() const override { return "Terminal"; }
	bool has_main_screen() const override { return false; }
	virtual void make_visible(bool p_visible) override;

	TerminalPlugin();
	~TerminalPlugin();
};

#endif // TOOLS_ENABLED
