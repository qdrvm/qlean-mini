import os
import sys
import subprocess
import re

# Paths relative to the project root
PROJECT_DIR = os.path.abspath(os.path.dirname(__file__))
SRC_DIR = os.path.join(PROJECT_DIR, "core")
TEST_DIR = os.path.join(PROJECT_DIR, "test")
MOCK_DIR = os.path.join(TEST_DIR, "mock")
SRC_REFLECTION = "core"  # Set to "" if no reflection component, or to SRC_DIR if it matches the source directory name
CLANG_FORMAT = "/usr/local/bin/clang-format"  # Path to the clang-format executable


def log(message):
    print(f"[DEBUG] {message}")


def find_clang_format():
    log(f"Searching for .clang-format in project directory: {PROJECT_DIR}")
    clang_format_path = os.path.join(PROJECT_DIR, '.clang-format')
    if os.path.isfile(clang_format_path):
        log(f"Found .clang-format at: {clang_format_path}")
        return clang_format_path
    log("No .clang-format file found.")
    return None


def find_main_file(file_path):
    file_name = os.path.basename(file_path)
    file_dir = os.path.dirname(file_path)
    name, ext = os.path.splitext(file_name)

    log(f"Finding main file for: {file_path}")

    def search_candidates(file_dir, base_name, suffix):
        candidates = []

        candidates.append(os.path.join(file_dir, base_name + suffix + ".hpp"))
        candidates.append(os.path.join(file_dir, base_name + ".hpp"))

        if os.path.basename(file_dir) == "impl":
            parent_dir = os.path.dirname(file_dir)
            candidates.append(os.path.join(parent_dir, base_name + ".hpp"))

        log(f"search: dir={file_dir} base={base_name} suffix={suffix}")
        return candidates

    def find_first_existing(candidates):
        # Return the first existing file from the candidate list
        log(f"Evaluating candidates: {candidates}")
        for candidate in candidates:
            if candidate and os.path.isfile(candidate):
                log(f"Selected candidate: {candidate}")
                return candidate
        log("No candidates found.")
        return None

    seen = set()
    candidates = []
    if ext == ".cpp" or ext == ".hpp":
        if name.endswith("_mock") and MOCK_DIR in file_dir:
            base_name = name[:-5]  # Remove `_mock`
            candidates.extend(search_candidates(file_dir, base_name, "_mock"))
            src_dir = file_dir.replace(MOCK_DIR + "/" + SRC_REFLECTION, SRC_DIR)
            log(f"MOCK_DIR={MOCK_DIR}  SRC_REFLECTION={SRC_REFLECTION} +={MOCK_DIR + "/" + SRC_REFLECTION} SRC_DIR={SRC_DIR} src={src_dir}")
            candidates.extend(search_candidates(src_dir, base_name, ""))
            candidates = [x for x in candidates if not (x in seen or seen.add(x)) and x != file_path]
            return find_first_existing(candidates)

        if name.endswith("_test") and TEST_DIR in file_dir:
            base_name = name[:-5]  # Remove `_test`
            candidates.extend(search_candidates(file_dir, base_name, "_test"))
            src_dir = file_dir.replace(TEST_DIR + "/" + SRC_REFLECTION, SRC_DIR)
            candidates.extend(search_candidates(src_dir, "impl/" + base_name, "_impl"))
            src_dir = file_dir.replace(TEST_DIR + "/" + SRC_REFLECTION, SRC_DIR)
            candidates.extend(search_candidates(src_dir, "impl/" + base_name, ""))
            src_dir = file_dir.replace(TEST_DIR + "/" + SRC_REFLECTION, SRC_DIR)
            candidates.extend(search_candidates(src_dir, base_name, "_impl"))
            src_dir = file_dir.replace(TEST_DIR + "/" + SRC_REFLECTION, SRC_DIR)
            candidates.extend(search_candidates(src_dir, base_name, ""))
            candidates = [x for x in candidates if not (x in seen or seen.add(x)) and x != file_path]
            return find_first_existing(candidates)

        if name.endswith("_impl") and SRC_DIR in file_dir:
            base_name = name[:-5]  # Remove `_impl`
            candidates.extend(search_candidates(file_dir, base_name, "_impl"))
            candidates = [x for x in candidates if not (tuple(x) in seen or seen.add(tuple(x))) and x != file_path]
            return find_first_existing(candidates)

        base_name = name
        candidates.extend(search_candidates(file_dir, base_name, ""))
        candidates = [x for x in candidates if not (x in seen or seen.add(x)) and x != file_path]
        return find_first_existing(candidates)

    log("No main file found.")
    return None


def make_relative_to_base(base_path, target_path):
    return os.path.relpath(target_path, base_path)


def modify_clang_format(base_clang_format, main_file, output_clang_format):
    if TEST_DIR in main_file:
        relative_main_file = make_relative_to_base(TEST_DIR, main_file)
    elif MOCK_DIR in main_file:
        relative_main_file = make_relative_to_base(MOCK_DIR, main_file)
    else:
        relative_main_file = make_relative_to_base(SRC_DIR, main_file)

    log(f"Using relative path for IncludeIsMainRegex: {relative_main_file}")

    with open(base_clang_format, 'r') as file:
        config_lines = file.readlines()

    # include_regex = f"IncludeIsMainRegex: '^{re.escape(relative_main_file)}$'\n"
    # updated = False

    for i, line in enumerate(config_lines):
        line = line.replace("MAIN_INCLUDE_FILE", f"{relative_main_file}"
                            # f"{re.escape(relative_main_file)}"
                            , 1)
        config_lines[i] = line

        # if line.startswith("  - Regex: 'MAIN_INCLUDE_FILE'"):
        #     config_lines[i] = f"  - Regex: '^{re.escape(relative_main_file)}$'\n"
        # # if line.startswith("IncludeIsMainRegex:"):
        # #     config_lines[i] = include_regex
        # log(f"Line in config: [{config_lines[i]}]")
        #     # updated = True
        #     break

    # if not updated:
    #     config_lines.append(include_regex)

    with open(output_clang_format, 'w') as file:
        file.writelines(config_lines)
    log(f"Modified .clang-format written to: {output_clang_format}")


def format_file(file_path, clang_format_path):
    log(f"Formatting file: {file_path} using .clang-format: {clang_format_path}")
    subprocess.run([CLANG_FORMAT, "-i", f"--style=file:{clang_format_path}", file_path],
                   env={"CLANG_FORMAT_STYLE": clang_format_path}, check=True)


def main():
    if len(sys.argv) < 2:
        print("Usage: python script.py <path-to-cpp-file> ...")
        sys.exit(1)

    arguments = sys.argv

    for i, arg in enumerate(arguments):
        if i == 0: continue
        file_path = os.path.abspath(arg)
        log(f"Processing file: {file_path}")

        if not os.path.isfile(file_path):
            print("Error: The specified file does not exist.")
            sys.exit(1)

        base_clang_format = find_clang_format()
        if not base_clang_format:
            print("Error: No .clang-format file found in the project directory.")
            sys.exit(1)

        main_file = find_main_file(file_path)

        if main_file:
            log(f"Main file found: {main_file}")
            temp_clang_format = base_clang_format + ".tmp"
            modify_clang_format(base_clang_format, main_file, temp_clang_format)
            try:
                format_file(file_path, temp_clang_format)
            finally:
                if os.path.isfile(temp_clang_format):
                    # os.remove(temp_clang_format)
                    log(f"Temporary .clang-format file removed: {temp_clang_format}")
        else:
            log("No main file found. Using base .clang-format.")
            format_file(file_path, base_clang_format)


if __name__ == "__main__":
    main()
