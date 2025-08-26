def can_build(env, platform):
    if platform == "windows":
        # Only build on Windows with MSVC compiler
        return not (env.get("use_mingw", False) or env.get("use_llvm", False))
    return platform == "macos" or platform == "linuxbsd"


def configure(env):
    pass


def get_doc_classes():
    return ["GDTerm"]


def get_doc_path():
    return "doc_classes"
