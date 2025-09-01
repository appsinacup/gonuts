def can_build(env, platform):
    # Only build this module for editor builds
    target = env.get("target", "")
    print(f"GodotIDE can_build check: target={target}, platform={platform}")
    if target == "editor":
        env.module_add_dependencies("godot_ide", ["godot_wry"], True)
        return True
    return False


def configure(env):
    pass


def get_doc_classes():
    return [
        "GodotIDE",
    ]


def get_doc_path():
    return "doc_classes"
