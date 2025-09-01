def can_build(env, platform):
    return platform == "macos" or platform == "linuxbsd" or platform == "windows"


def configure(env):
    pass


def get_doc_classes():
    return ["GDTerm"]


def get_doc_path():
    return "doc_classes"
