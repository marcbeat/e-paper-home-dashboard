Import("env")
import subprocess


def get_git_short_sha():
    try:
        return (
            subprocess.check_output(
                ["git", "rev-parse", "--short", "HEAD"],
                cwd=env.subst("$PROJECT_DIR"),
                stderr=subprocess.DEVNULL,
                text=True,
            ).strip()
            or "unknown"
        )
    except Exception:
        return "unknown"


sha = get_git_short_sha()
env.Append(CPPDEFINES=[("BUILD_GIT_SHA", env.StringifyMacro(sha))])
