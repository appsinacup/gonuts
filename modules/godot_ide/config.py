def can_build(env, platform):
    env.module_add_dependencies("godot_ide", ["godot_wry"], True)
    return True


def configure(env):
    pass


def get_doc_classes():
    return [
        "GodotIDE",
    ]


def get_doc_path():
    return "doc_classes"
