# CI_WEB_ENFORCEMENT_REPORT.md

## Scope
- Branch: `codex/freeze-contract-plan`
- Mission: enforce the web companion quality lane in pull-request CI without weakening existing firmware gates

## Workflow Changes

### `.github/workflows/ci.yml`
- Added workflow-level `WEB_NODE_VERSION = 20.19.4` so CI matches the locally proven Node lane.
- Added `ci / web-quality` for:
  - `npm ci`
  - `npx vitest run`
  - `npm run build`
- Kept the existing firmware host-test and ESP-IDF firmware-build jobs intact.
- Added `ci / summary` so PRs get one clear status table for:
  - firmware host tests
  - web companion tests/build
  - ESP-IDF firmware build
- Added explicit summary text for the intentionally deferred lint gate.

### `.github/workflows/deploy-webapp.yml`
- Aligned Node setup with the same `WEB_NODE_VERSION = 20.19.4`.
- Added a short web deploy build summary so the deployment workflow uses the same documented build assumptions as CI.

### `.github/workflows/release.yml`
- Validated unchanged.
- Intentionally did not add the web test/build lane here because `ci.yml` now enforces it on PRs already, and duplicating the same web checks in the release preflight would add avoidable runtime.

## Deferred Checks

### Web lint enforcement
- Status: deferred
- Reason: `cd charm-web-companion && npm run lint` currently fails before repo lint rules execute because `@rushstack/eslint-patch` does not recognize the installed ESLint 9 caller.
- Target date: `2026-04-20`
- Exit condition: either update/remove the patch path so `npm run lint` runs cleanly under Node 20, or replace it with a stable supported lint invocation.

### Release-workflow web duplication
- Status: deferred
- Reason: PR CI now enforces the web lane in `ci.yml`; duplicating the same `npm ci + vitest + build` sequence in `release.yml` would increase runtime without adding distinct PR coverage.
- Target date: `2026-04-20`
- Exit condition: revisit once the team decides whether PR/release workflows should be consolidated or kept split by purpose.

## Local Validation

### Workflow file validation
- Command:
  - `ruby -e 'require "yaml"; [".github/workflows/ci.yml", ".github/workflows/deploy-webapp.yml", ".github/workflows/release.yml"].each { |path| YAML.load_stream(File.read(path)); puts "OK #{path}" }'`
- Result:
  - `OK .github/workflows/ci.yml`
  - `OK .github/workflows/deploy-webapp.yml`
  - `OK .github/workflows/release.yml`

### Local dry-run command matrix

| Lane | Command | Result | Notes |
| --- | --- | --- | --- |
| Firmware host tests | `cmake -S tests/unit -B build/unit && cmake --build build/unit --parallel && ctest --test-dir build/unit --output-on-failure` | pass | `20/20` tests passed |
| Web tests | `cd charm-web-companion && npx vitest run` | pass | `60` passed under Node `20.19.4` |
| Web build | `cd charm-web-companion && npm run build` | pass | production static build succeeded under Node `20.19.4` |
| Web lint | `cd charm-web-companion && npm run lint` | deferred/failing | blocked by current `@rushstack/eslint-patch` + ESLint 9 incompatibility |
| ESP-IDF local build | `source /path/to/esp-idf/export.sh && idf.py build` | pass | local shell proof now matches the enforced GitHub Actions firmware-build lane |

## Summary Output Shape
- The PR CI summary now produces a single lane table in `ci / summary`.
- That summary explicitly reports:
  - firmware host-test result
  - web-lane result
  - ESP-IDF firmware-build result
  - deferred lint note with date and reason

## Expected PR Outcome
- A pull request now fails if:
  - host-side firmware unit tests fail
  - web Vitest fails
  - web production build fails
  - ESP-IDF firmware build fails
- A pull request does not currently fail on lint until the deferred lint issue is resolved by `2026-04-20`.
