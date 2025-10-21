Shadow example
===============

Quick steps

1) Install Shadow

Follow the official guide: https://github.com/shadow/shadow

2) Generate shadow.yaml from template

Generate the `shadow.yaml` file from the template by replacing the `@PROJECT_ROOT@` placeholder with your project root:

```bash
cd /path/to/qlean-mini
sed "s|@PROJECT_ROOT@|$(pwd)|g" shadow/shadow.yaml.in > shadow/shadow.yaml
```

3) Run the example

```bash
rm -rf shadow.data && shadow --progress true --parallelism $(nproc) shadow/shadow.yaml
```

Notes
- The `shadow.yaml.in` file is a template with `@PROJECT_ROOT@` placeholders that need to be replaced with actual paths.
- Ensure the `qlean` executable path points to your built binary (build it if needed).
- If you run Shadow from a different directory, make all paths in `shadow/shadow.yaml` absolute.
