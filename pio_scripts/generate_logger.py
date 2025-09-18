import os
import shutil
import subprocess

Import("env")

# example_target = env.GetProjectOption("custom_example_target")

out_dir = "build/pio_lib/min_logger"
if os.path.exists(out_dir):
    shutil.rmtree(out_dir)

os.makedirs("build/pio_lib", exist_ok=True)

shutil.copytree("src/min_logger", out_dir)

cmd = [
    "uv",
    "--project",
    "python",
    "run",
    "min-logger-builder",
    "examples/esp32_pio",
    "--root_paths",
    ".",
    "--header_output",
    out_dir + "/_min_logger_gen.h",
]

subprocess.run(cmd, check=True)
