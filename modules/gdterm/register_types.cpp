#include "register_types.h"
#include "gdterm/gdterm.h"
#ifdef TOOLS_ENABLED
#include "gdterm/terminal_plugin.h"
#endif

#include "core/object/class_db.h"

void initialize_gdterm_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		ClassDB::register_class<GDTerm>();
#ifdef TOOLS_ENABLED
	} else if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		ClassDB::register_class<TerminalPlugin>();
		EditorPlugins::add_by_type<TerminalPlugin>();
#endif
	}
}

void uninitialize_gdterm_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
}
