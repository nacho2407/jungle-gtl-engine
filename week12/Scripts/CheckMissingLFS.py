# -*- coding: utf-8 -*-
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path


def run(args: list[str], *, cwd: Path, capture: bool = False) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        args,
        cwd=cwd,
        text=True,
        encoding="utf-8",
        errors="replace",
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
    )


def command_exists(args: list[str], cwd: Path) -> bool:
    try:
        result = run(args, cwd=cwd, capture=True)
    except FileNotFoundError:
        return False
    return result.returncode == 0


def find_python_commands() -> list[list[str]]:
    candidates = [[sys.executable], ["py", "-3"], ["python"]]
    commands: list[list[str]] = []
    seen: set[str] = set()

    for candidate in candidates:
        try:
            result = subprocess.run(
                [*candidate, "--version"],
                text=True,
                encoding="utf-8",
                errors="replace",
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
        except FileNotFoundError:
            continue

        if result.returncode == 0:
            key = " ".join(candidate).lower()
            if key not in seen:
                seen.add(key)
                commands.append(candidate)

    return commands


def git_filter_repo_module_path(python_cmd: list[str]) -> Path | None:
    result = subprocess.run(
        [*python_cmd, "-c", "import git_filter_repo; print(git_filter_repo.__file__)"],
        text=True,
        encoding="utf-8",
        errors="replace",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        return None

    module_path = result.stdout.strip()
    if not module_path:
        return None

    path = Path(module_path)
    return path if path.exists() else None


def install_filter_repo(repo_root: Path) -> tuple[list[str], Path] | None:
    print()
    print("git-filter-repo가 없어 설치를 시도합니다.")

    python_commands = find_python_commands()
    if not python_commands:
        print("Python을 찾을 수 없어 git-filter-repo를 자동 설치할 수 없습니다.")
        print("Python을 설치하거나 PATH에 추가한 뒤 다시 실행하세요.")
        return None

    for python_cmd in python_commands:
        print()
        print("사용할 Python:")
        print("  " + " ".join(python_cmd))

        existing_module = git_filter_repo_module_path(python_cmd)
        if existing_module is not None:
            print(f"이미 설치된 git-filter-repo 모듈을 사용합니다: {existing_module}")
            return python_cmd, existing_module

        pip_check = run([*python_cmd, "-m", "pip", "--version"], cwd=repo_root, capture=True)
        if pip_check.returncode != 0:
            print("pip가 없어 ensurepip로 설치를 시도합니다.")
            ensurepip_cmd = [*python_cmd, "-m", "ensurepip", "--upgrade"]
            print("ensurepip 명령:")
            print("  " + " ".join(ensurepip_cmd))
            ensurepip_result = run(ensurepip_cmd, cwd=repo_root)
            if ensurepip_result.returncode != 0:
                print("이 Python에서는 pip를 준비하지 못했습니다. 다음 Python 후보를 확인합니다.")
                continue

        install_cmd = [*python_cmd, "-m", "pip", "install", "--user", "git-filter-repo"]
        print("설치 명령:")
        print("  " + " ".join(install_cmd))

        install_result = run(install_cmd, cwd=repo_root)
        if install_result.returncode != 0:
            print("이 Python으로 git-filter-repo 설치에 실패했습니다. 다음 Python 후보를 확인합니다.")
            continue

        module_path = git_filter_repo_module_path(python_cmd)
        if module_path is not None:
            print(f"git-filter-repo 설치가 완료되었습니다: {module_path}")
            return python_cmd, module_path

        print("설치는 되었지만 git_filter_repo 모듈 파일을 찾지 못했습니다. 다음 Python 후보를 확인합니다.")

    print("git-filter-repo 설치에 실패했습니다. Python, 네트워크 또는 pip 설정을 확인하세요.")
    return None


def find_installed_filter_repo_runner() -> tuple[list[str], Path] | None:
    for python_cmd in find_python_commands():
        module_path = git_filter_repo_module_path(python_cmd)
        if module_path is not None:
            return python_cmd, module_path

    return None


def collect_missing_lfs_paths(repo_root: Path) -> list[str] | None:
    print("[1/3] Git LFS 추적 경로를 스캔하는 중...")
    result = run(["git", "lfs", "ls-files", "--all", "-n"], cwd=repo_root, capture=True)
    if result.returncode != 0:
        print("오류: Git LFS 경로 목록을 가져오지 못했습니다.")
        if result.stderr:
            print(result.stderr.strip())
        return None

    print("[2/3] 현재 로컬 파일에 경로가 존재하는지 확인하는 중...")
    lfs_paths = sorted({line.strip() for line in result.stdout.splitlines() if line.strip()})
    missing_paths = [path for path in lfs_paths if not (repo_root / Path(path)).exists()]

    print("[3/3] 검사 완료.")
    print()
    print(f"누락된 LFS 경로 수: {len(missing_paths)}")
    print()
    return missing_paths


def confirm_rewrite(command_text: str) -> bool:
    print("경고:")
    print("  다음 단계는 main 브랜치의 로컬 커밋 기록을 다시 쓸 수 있습니다.")
    print("  main 기록을 수정하면 기존 클론, 브랜치, Pull Request, 팀원의 로컬 작업이")
    print("  더 이상 같은 커밋 ID를 공유하지 않을 수 있습니다.")
    print("  히스토리를 강제 푸시하기 전에 반드시 팀과 조율하고 백업을 만들어 두세요.")
    print()
    print("실행 예정 명령어:")
    print(f"  {command_text}")
    print()
    print("명령어 설명:")
    print("  위 명령은 설치된 Python의 git_filter_repo 모듈 파일을 직접 실행합니다.")
    print("  Git의 외부 서브커맨드 탐색을 쓰지 않아서 PATH/shim 문제를 피합니다.")
    print("  --force는 fresh clone이 아니라는 보호장치를 넘기고 히스토리 수정을 강제로 진행합니다.")
    print("  --invert-paths는 지정한 경로를 삭제 대상으로 해석합니다.")
    print("  --paths-from-file은 임시 파일에 들어 있는 누락 경로 목록을 읽습니다.")
    print("  이 작업은 로컬 히스토리를 다시 쓰며, 이후 main에 반영하려면 force push가 필요합니다.")
    print()

    while True:
        answer = input("진행하시겠습니까? [Y/N]: ").strip().lower()
        if answer == "y":
            return True
        if answer == "n" or answer == "":
            return False
        print("Y 또는 N만 입력해주세요.")


def print_next_steps() -> None:
    print()
    print("로컬 히스토리 수정이 완료되었습니다.")
    print()
    print("결과를 확인한 뒤 Git Bash에서 실행할 다음 명령:")
    print()
    print("  git status")
    print("  git log --oneline --decorate -5")
    print("  git remote -v")
    print()
    print("filter-repo 실행 후 origin remote가 사라졌다면 다시 추가하세요:")
    print()
    print("  git remote add origin <YOUR_REPOSITORY_URL>")
    print()
    print("팀과 조율했고 main 기록을 교체할 준비가 되었을 때만 실행하세요:")
    print()
    print("  git push origin main --force-with-lease")
    print()
    print("강제 푸시 후에는 팀원들에게 다시 클론하거나 수정된 main으로 hard reset 하도록 안내하세요.")


def main() -> int:
    script_path = Path(__file__).resolve()
    repo_root = script_path.parents[1]

    try:
        os.chdir(repo_root)
    except OSError:
        print("오류: 저장소 루트 폴더로 이동할 수 없습니다.")
        return 1

    if not command_exists(["git", "rev-parse", "--is-inside-work-tree"], repo_root):
        print("오류: Git 저장소가 아닙니다.")
        return 1

    if not command_exists(["git", "lfs", "version"], repo_root):
        print("오류: Git LFS가 설치되어 있지 않거나 PATH에서 찾을 수 없습니다.")
        return 1

    missing_paths = collect_missing_lfs_paths(repo_root)
    if missing_paths is None:
        return 1

    if not missing_paths:
        print("누락된 LFS 포인터 경로가 없습니다.")
        return 0

    print("누락된 LFS 포인터 경로:")
    for path in missing_paths:
        print(path)
    print()

    with tempfile.NamedTemporaryFile("w", encoding="utf-8", newline="\n", delete=False, suffix=".txt") as temp_file:
        temp_path = Path(temp_file.name)
        temp_file.write("\n".join(missing_paths))
        temp_file.write("\n")

    try:
        runner = find_installed_filter_repo_runner()
        if runner is not None:
            python_cmd, module_path = runner
            command = [*python_cmd, str(module_path), "--force", "--invert-paths", "--paths-from-file", str(temp_path)]
            command_text = " ".join(f'"{part}"' if " " in part else part for part in command)
        else:
            command = []
            command_text = f"<설치된 Python> <git_filter_repo.py> --force --invert-paths --paths-from-file \"{temp_path}\""

        if not confirm_rewrite(command_text):
            print()
            print("취소했습니다. Git 기록은 변경되지 않았습니다.")
            return 0

        if runner is None:
            runner = install_filter_repo(repo_root)
            if runner is None:
                return 1
            python_cmd, module_path = runner
            command = [*python_cmd, str(module_path), "--force", "--invert-paths", "--paths-from-file", str(temp_path)]

        print()
        print("로컬 Git 기록을 다시 쓰는 중...")
        result = run(command, cwd=repo_root)
        if result.returncode != 0:
            print("오류: git-filter-repo 실행에 실패했습니다.")
            return 1

        print_next_steps()
        return 0
    finally:
        try:
            temp_path.unlink(missing_ok=True)
        except OSError:
            pass


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print()
        print("사용자가 작업을 중단했습니다.")
        sys.exit(1)
