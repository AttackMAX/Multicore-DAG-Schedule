import subprocess, json, os

project_root = "/home/administrator/projects/Multicore-DAG-Schedule"

result = subprocess.run(
    ["bazel", "aquery", 'mnemonic("CppCompile", //...)', "--output=jsonproto"],
    cwd=project_root, capture_output=True, text=True
)

if result.returncode != 0:
    print("bazel aquery failed:", result.stderr)
    exit(1)

data = json.loads(result.stdout)

flags_with_arg = {"-o", "-MF", "-c"}
remove_prefixes = ("-frandom-seed", "-MD", "-fdebug-prefix-map")

compile_commands = []
seen = set()

for action in data.get("actions", []):
    if action.get("mnemonic") != "CppCompile":
        continue

    args = list(action.get("arguments", []))

    # Extract source file
    try:
        c_idx = args.index("-c")
        src_file = args[c_idx + 1]
    except (ValueError, IndexError):
        continue

    if src_file in seen:
        continue
    seen.add(src_file)

    filtered = []
    skip = 0
    for a in args:
        if skip > 0:
            skip -= 1
            continue
        if a in flags_with_arg:
            skip = 1
            continue
        if any(a.startswith(p) for p in remove_prefixes):
            continue
        # Skip compiler binary path (first arg, ends with gcc or clang++)
        if a.endswith("/gcc") or a.endswith("/clang++") or a.endswith("/clang"):
            continue
        filtered.append(a)

    cmd = "clang++ " + " ".join(filtered)

    compile_commands.append({
        "directory": project_root,
        "file": src_file,
        "command": cmd,
    })

output_path = os.path.join(project_root, "compile_commands.json")
with open(output_path, "w") as f:
    json.dump(compile_commands, f, indent=2)

print(f"Generated {output_path} with {len(compile_commands)} entries")
for cc in compile_commands:
    print(f"  {cc['file']}")
