---
name: publish
description: Build and publish Fractory to itch.io with version tagging. Use when the user says "publish", "release", "deploy to itch", or "push a build".
---

# Publish Fractory

Builds all available platforms and pushes them to itch.io using butler, then tags the release in git.

## Steps

### 1. Get the API key

Check if `BUTLER_API_KEY` is set in the environment. If not, ask the user to provide their itch.io API key. Once provided, export it for the session:

```bash
export BUTLER_API_KEY="<key>"
```

### 2. Read the version

Read the version from the `VERSION` file at the project root. Display it to the user and confirm they want to publish this version. If the user wants to bump the version, edit the `VERSION` file first.

### 3. Confirm with the user

Show a summary before proceeding:

```
Version: X.Y.Z
Target:  robspsj/fractory
Platforms: osx, html5
Git tag: vX.Y.Z
```

Ask the user to confirm. Also ask if they want to bump the version first.

### 4. Build and push

Run the deploy script:

```bash
BUTLER_API_KEY="<key>" ./tools/deploy_itch.sh
```

This builds both macOS and web, then pushes to itch.io with the version from `VERSION`.

### 5. Git tag

After a successful push, create and push a git tag:

```bash
git tag "vX.Y.Z"
git push origin "vX.Y.Z"
```

Where `X.Y.Z` is the version from the `VERSION` file.

If the tag already exists, warn the user and ask if they want to force-update it or skip tagging.

### 6. Summary

Report the results:

- Which platforms were pushed
- The git tag created
- Link to the itch.io page: https://robspsj.itch.io/fractory

## Notes

- The deploy script skips `linux` and `windows` with a message — those need CI or building on the target OS.
- The `VERSION` file is the single source of truth for the version number.
- After the first web push, the user should set the itch.io page type to "HTML" in the edit page.
