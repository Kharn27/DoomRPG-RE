Import("env")

from os.path import join


project_dir = env.subst("$PROJECT_DIR")
engine_dir = join(project_dir, "..", "src")

env.Append(CPPPATH=[join(project_dir, "include"), engine_dir])

# The desktop entry point and its SDL/audio/ZIP implementations are replaced by
# the small ESP32 compatibility layer in this PlatformIO project.
env.BuildSources(
    join(env.subst("$BUILD_DIR"), "doomrpg_engine"),
    engine_dir,
    src_filter=[
        "+<*.c>",
        "-<Main.c>",
        "-<SDL_Video.c>",
        "-<Sound.c>",
        "-<Z_Zone.c>",
    ],
)
