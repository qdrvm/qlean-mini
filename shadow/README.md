Shadow example
===============

Quick steps

1) Install Shadow

Follow the official guide: https://github.com/shadow/shadow

2) Update paths

Replace the placeholder `/path/to/qlean-mini` in `shadow/shadow.yaml` with your project root. From the project root run:

```bash
cd /path/to/qlean-mini
sed -i "s|/path/to/qlean-mini|$(pwd)|g" shadow/shadow.yaml
```

3) Run the example

```bash
shadow --progress true --parallelism $(nproc) shadow/shadow.yaml
```

4) Cleanup

Remove generated state before re-running:

```bash
rm -rf shadow.data
```

Notes
- Ensure the `qlean` executable path in `shadow/shadow.yaml` points to your built binary (build it if needed).
- If you run Shadow from a different directory, make all paths in `shadow/shadow.yaml` absolute.
